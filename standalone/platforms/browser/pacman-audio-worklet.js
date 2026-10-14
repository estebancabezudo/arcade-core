// Architecture and design
//
// This same-origin AudioWorklet is the CSP-safe playback boundary for browser
// distributions. The packager also embeds these exact bytes for legacy consumers
// that do not provide audioWorkletHref.

class PacmanPCM extends AudioWorkletProcessor {
  constructor() {
    super(); this.ring=new Int16Array(32768); this.r=0; this.w=0; this.n=0;
    this.frac=0; this.ratio=48000/sampleRate; this.playing=false;
    this.startAt=0; this.under=0; this.over=0; this.blocks=0;
    this.port.onmessage=({data:d})=>{
      if(d.type==='reset') {
        this.r=this.w=this.n=0; this.frac=0; this.startAt=d.frame; this.playing=true;
      } else if(d.type==='stop') { this.playing=false; this.r=this.w=this.n=0; }
      else if(d.type==='pcm') {
        const a=new Int16Array(d.buffer);
        if(this.n+d.count>32768){this.over++; this.r=this.w=this.n=0;this.frac=0;}
        for(let i=0;i<d.count;i++){this.ring[this.w]=a[i];this.w=(this.w+1)&32767;}
        this.n+=d.count;
        this.port.postMessage({type:'recycle',buffer:d.buffer},[d.buffer]);
      }
    };
  }
  process(inputs,outputs) {
    const out=outputs[0][0]; if(!out)return true;
    let starved=false;
    for(let i=0;i<out.length;i++) {
      if(!this.playing||currentFrame+i<this.startAt){out[i]=0;continue;}
      if(this.ratio===1) {
        if(!this.n){out[i]=0;starved=true;continue;}
        out[i]=this.ring[this.r]/32768;this.r=(this.r+1)&32767;this.n--;
      } else {
        if(this.n<Math.ceil(this.ratio)+1){out[i]=0;starved=true;continue;}
        const a=this.ring[this.r],b=this.ring[(this.r+1)&32767];
        out[i]=(a+(b-a)*this.frac)/32768;
        this.frac+=this.ratio;
        const take=Math.floor(this.frac);this.frac-=take;this.r=(this.r+take)&32767;this.n-=take;
      }
    }
    if(starved)this.under++;
    if(++this.blocks===128){this.blocks=0;this.port.postMessage({type:'stats',queued:this.n,underruns:this.under,overruns:this.over});}
    return true;
  }
}
registerProcessor('pacman-pcm-v1',PacmanPCM);
