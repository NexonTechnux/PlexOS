/* PlexOS website JS — particles, typewriter, reveal, live releases */
(function(){
  "use strict";
  const API="https://api.github.com/repos/NexonTechnux/PlexOS/releases/latest";
  const RELEASES="https://github.com/NexonTechnux/PlexOS/releases";

  /* ---------- particle field (hero) ---------- */
  function particles(){
    const c=document.getElementById("fx");
    if(!c) return;
    const ctx=c.getContext("2d");
    let W,H,ps=[];
    function resize(){W=c.width=innerWidth;H=c.height=innerHeight;}
    resize();
    addEventListener("resize",resize);
    const N=Math.min(90,Math.floor(innerWidth/16));
    for(let i=0;i<N;i++){
      ps.push({x:Math.random()*W,y:Math.random()*H,r:Math.random()*1.6+.4,
        vx:(Math.random()-.5)*.35,vy:(Math.random()-.5)*.35,
        hue:Math.random()<.5?195:265,a:Math.random()*.5+.2});
    }
    (function loop(){
      ctx.clearRect(0,0,W,H);
      for(const p of ps){
        p.x+=p.vx;p.y+=p.vy;
        if(p.x<0)p.x=W;if(p.x>W)p.x=0;
        if(p.y<0)p.y=H;if(p.y>H)p.y=0;
        ctx.beginPath();
        ctx.arc(p.x,p.y,p.r,0,7);
        ctx.fillStyle=`hsla(${p.hue},90%,70%,${p.a})`;
        ctx.fill();
      }
      for(let i=0;i<ps.length;i++)for(let j=i+1;j<ps.length;j++){
        const a=ps[i],b=ps[j],dx=a.x-b.x,dy=a.y-b.y,d2=dx*dx+dy*dy;
        if(d2<12000){
          ctx.strokeStyle=`hsla(200,90%,70%,${.16*(1-d2/12000)})`;
          ctx.lineWidth=.6;
          ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.stroke();
        }
      }
      requestAnimationFrame(loop);
    })();
  }

  /* ---------- typewriter ---------- */
  function typewriter(el,texts,t0){
    let i=0,ti=0,txt="",del=false;
    const login="plexos@qemu:~$ ";
    function step(){
      const cur=texts[ti];
      if(!del){
        i++;
        el.innerHTML=login+cur.slice(0,i)+'<span class="cursor">&nbsp;</span>';
        if(i===cur.length){del=true;setTimeout(step,t0);return;}
        setTimeout(step,26);
      }else{
        i--;
        if(i===0){del=false;ti=(ti+1)%texts.length;i=0;}
        el.innerHTML=login+cur.slice(0,i)+'<span class="cursor">&nbsp;</span>';
        setTimeout(step,12);
      }
    }
    step();
  }

  /* ---------- terminal app (once, then blinking cursor) ---------- */
  function termDemo(){
    document.querySelectorAll(".term[data-termlines]").forEach(term=>{
      const body=term.querySelector(".body");
      const lines=JSON.parse(term.dataset.termlines);
      let li=0;
      body.innerHTML='';
      (function print(chnk){
        if(chnk){
          const span=document.createElement("span");
          span.className=chnk.cls||"";
          span.textContent=chnk.t;
          body.appendChild(span);
        }
        if(li<lines.length){
          const l=lines[li++];
          const el=document.createElement("div");
          el.innerHTML=l.html;
          body.appendChild(el);
          const dt=l.delay||60;
          setTimeout(()=>print(null),dt);
        }else{
          body.appendChild(lineSpan('p',"plexos@qemu:~$ "));
          body.appendChild(document.createTextNode(" "));
          const cu=document.createElement("span");cu.className="cursor";cu.innerHTML="&nbsp;";
          body.appendChild(cu);
        }
      })(null);
    });
  }
  function lineSpan(cls,t){const s=document.createElement("span");s.className=cls;s.textContent=t;return s;}

  /* ---------- reveal on scroll ---------- */
  function reveal(){
    const io=new IntersectionObserver(es=>{
      es.forEach(e=>{if(e.isIntersecting){e.target.classList.add("in");io.unobserve(e.target);}});
    },{threshold:.12});
    document.querySelectorAll(".reveal").forEach(el=>io.observe(el));
  }

  /* ---------- live download from releases ---------- */
  function fmtSize(b){
    if(!b&&b!==0) return "—";
    const u=["B","KB","MB","GB"];let i=0;
    while(b>=1024&&i<u.length-1){b/=1024;i++;}
    return b.toFixed(b<10&&i>0?2:0)+" "+u[i];
  }
  function dlRow(r){
    const box=document.getElementById("dload");
    const assets=r.assets&&r.assets.length?r.assets:[
      {name:"plexos.iso",browser_download_url:RELEASES+"/download/"+r.tag_name+"/plexos.iso",size:0}
    ];
    box.innerHTML="";
    for(const a of assets){
      const row=document.createElement("div");
      row.className="drow reveal in";
      row.innerHTML=
        '<div class="meta">'+
          '<div class="v">'+r.tag_name+' · live release · '+new Date(r.published_at).toLocaleDateString()+'</div>'+
          '<div class="n">'+a.name+'</div>'+
          '<div class="d">'+fmtSize(a.size)+' &middot; '+r.name+'</div>'+
        '</div>'+
        '<a class="btn btn-grad" href="'+a.browser_download_url+'" download>⬇&nbsp;Download</a>';
      box.appendChild(row);
    }
    const more=document.createElement("div");
    more.className="errdl";more.style.marginTop="6px";
    more.innerHTML='All versions on <a href="'+RELEASES+'" style="color:var(--acc);margin-left:5px">GitHub Releases</a>';
    box.appendChild(more);
  }
  function loadRelease(){
    const box=document.getElementById("dload");
    if(!box) return;
    box.innerHTML='<div class="spinner" title="Loading latest release…"></div>';
    fetch(API,{headers:{Accept:"application/vnd.github+json"}})
      .then(r=>{if(!r.ok)throw new Error(r.status);return r.json();})
      .then(dlRow)
      .catch(()=>{
        box.innerHTML=
          '<div class="errdl">Could not reach the GitHub API.</div>';
        const row=document.createElement("div");
        row.className="drow reveal in";
        row.innerHTML=
          '<div class="meta"><div class="n">plexos.iso</div><div class="d">latest QEMU release</div></div>'+
          '<a class="btn btn-grad" href="'+RELEASES+'">⬇&nbsp;Open Releases</a>';
        box.appendChild(row);
      });
  }

  /* ---------- init ---------- */
  addEventListener("DOMContentLoaded",()=>{
    particles();
    reveal();
    loadRelease();
    const tw=document.getElementById("typewrap");
    if(tw){
      const texts=JSON.parse(tw.dataset.texts);
      typewriter(tw,texts,2400);
    }else{
      termDemo();
    }
  });
})();