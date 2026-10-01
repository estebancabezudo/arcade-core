#!/usr/bin/env python3
"""Render both Markdown READMEs into local HTML pages with a playable arcade."""

from html import escape
from pathlib import Path
import re
import sys
import unicodedata
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    try:
        from markdown_it import MarkdownIt
    except ImportError:
        print("Install the optional docs/requirements.txt dependency to render READMEs.",
              file=sys.stderr)
        return 1
    output = ROOT / "build/docs"
    output.mkdir(parents=True, exist_ok=True)
    parser = MarkdownIt("commonmark", {"html": True}).enable("table")
    for filename, language in (("README.md", "en"), ("README.es.md", "es")):
        markdown = (ROOT / filename).read_text(encoding="utf-8")
        # GitHub keeps a static preview; only the generated website gets live HTML.
        preview = r"<!-- PACMAN_DEMO:BEGIN -->[\s\S]*?<!-- PACMAN_DEMO:END -->"
        game = (
            '<iframe src="build/standalone/browser/index.html" '
            'title="Pac-Man arcade" width="448" height="576" '
            'allow="autoplay; fullscreen" allowfullscreen></iframe>'
        )
        markdown, count = re.subn(preview, game, markdown)
        if count != 1:
            print(f"Expected one arcade preview marker in {filename}.", file=sys.stderr)
            return 1
        tokens = parser.parse(markdown)
        used = {}
        for index, token in enumerate(tokens):
            if token.type != "heading_open":
                continue
            title = tokens[index + 1].content.lower().replace("`", "")
            title = unicodedata.normalize("NFC", title)
            slug = re.sub(r"[^\w\- ]", "", title, flags=re.UNICODE).replace(" ", "-")
            count = used.get(slug, 0)
            used[slug] = count + 1
            token.attrSet("id", slug if not count else f"{slug}-{count}")
        html = parser.renderer.render(tokens, parser.options, {})

        def rewrite(match):
            attribute, value = match.groups()
            if not value or value.startswith(("#", "/")) or urlsplit(value).scheme:
                return match.group(0)
            path, separator, anchor = value.partition("#")
            if path in ("README.md", "README.es.md"):
                value = path.replace(".md", ".html") + separator + anchor
            elif path in ("build/docs/README.html", "build/docs/README.es.html"):
                value = Path(path).name + separator + anchor
            else:
                value = "../../" + value
            return f'{attribute}="{value}"'

        html = re.sub(r'(href|src)="([^"]*)"', rewrite, html)
        page = f'''<!doctype html>
<html lang="{language}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>arcade-core — {escape(language.upper())}</title>
  <style>
    html {{ color-scheme: light dark; scroll-behavior: smooth; }}
    body {{ max-width: 960px; margin: 0 auto; padding: 24px; font: 16px/1.65 system-ui,sans-serif; }}
    h1,h2,h3 {{ line-height: 1.25; margin-top: 2em; }}
    pre {{ overflow: auto; padding: 16px; background: #8882; border-radius: 4px; }}
    code {{ font-family: ui-monospace,monospace; font-size: .9em; }}
    table {{ border-collapse: collapse; width: 100%; }}
    th,td {{ border-bottom: 1px solid #8886; padding: 8px; text-align: left; }}
    iframe {{ display: block; width: 448px; height: 576px; border: 0; max-width: 100%; }}
    img {{ max-width: 100%; height: auto; image-rendering: pixelated; }}
    @media(max-width:500px) {{ body {{ padding: 12px; }} iframe {{ height: auto; aspect-ratio: 224/288; }} }}
  </style>
</head>
<body>
{html}
</body>
</html>
'''
        target = output / filename.replace(".md", ".html")
        target.write_text(page, encoding="utf-8")
        print(f"Rendered {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
