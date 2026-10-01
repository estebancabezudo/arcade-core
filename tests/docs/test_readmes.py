"""Check the static GitHub preview and locally rendered playable READMEs."""

from html.parser import HTMLParser
from pathlib import Path
import subprocess
import sys
import unittest

from markdown_it import MarkdownIt

ROOT = Path(__file__).resolve().parents[2]


class Elements(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.elements = []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        self.elements.append((tag, dict(attrs)))


class ReadmeTests(unittest.TestCase):
    def test_github_markdown_has_static_preview_only(self):
        parser = MarkdownIt('commonmark', {'html': True})
        for filename in ('README.md', 'README.es.md'):
            with self.subTest(filename=filename):
                text = (ROOT / filename).read_text(encoding='utf-8')
                self.assertEqual(text.count('<!-- PACMAN_DEMO:BEGIN -->'), 1)
                self.assertEqual(text.count('<!-- PACMAN_DEMO:END -->'), 1)
                markup = parser.render(text)
                elements = Elements(markup).elements
                self.assertFalse(any(tag in ('iframe', 'script') for tag, _ in elements))
                self.assertEqual(sum(tag == 'img' and attrs.get('src') ==
                                     'docs/images/pacman.png' for tag, attrs in elements), 1)
                self.assertFalse(any('YOUR-USER' in attrs.get('href', '')
                                     for _, attrs in elements))
                self.assertFalse(any(attrs.get('href', '').endswith('index.html')
                                     for _, attrs in elements))

    def test_html_has_one_playable_portable_iframe(self):
        for filename in ('README.html', 'README.es.html'):
            with self.subTest(filename=filename):
                path = ROOT / 'build/docs' / filename
                text = path.read_text(encoding='utf-8')
                frames = [attrs for tag, attrs in Elements(text).elements if tag == 'iframe']
                self.assertEqual(len(frames), 1)
                target = (path.parent / frames[0]['src']).resolve()
                self.assertEqual(target, ROOT / 'build/standalone/browser/index.html')
                self.assertTrue(target.is_file())
                self.assertIn('screenOnly: true', target.read_text(encoding='utf-8'))
                self.assertTrue((target.parent / 'pacman.js').is_file())
                self.assertNotIn('PACMAN_DEMO:', text)
                self.assertNotIn('<img', text)

    def test_html_language_and_encoding(self):
        for filename, lang in (('README.html', 'en'), ('README.es.html', 'es')):
            with self.subTest(filename=filename):
                text = (ROOT / 'build/docs' / filename).read_text(encoding='utf-8')
                elements = Elements(text).elements
                self.assertIn(('html', {'lang': lang}), elements)
                self.assertIn(('meta', {'charset': 'utf-8'}), elements)
                self.assertNotIn('\ufffd', text)

    def test_renderer_is_deterministic_from_another_directory(self):
        names = ('README.html', 'README.es.html')
        before = [(ROOT / 'build/docs' / name).read_bytes() for name in names]
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/render-readmes.py')],
                                cwd=ROOT.parent, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        after = [(ROOT / 'build/docs' / name).read_bytes() for name in names]
        self.assertEqual(before, after)


if __name__ == '__main__':
    unittest.main()
