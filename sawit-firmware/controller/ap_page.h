// ap_page.h — halaman mode AP (192.168.4.1): SATU HTML untuk Controller + Sensor node. Tema putih-biru sama dengan web AirTracking.
// DIHASILKAN dari ../ap_page.html (sumber utama; bisa dibuka di browser, tambahkan ?demo untuk data contoh).
// Tanpa file/font eksternal (AP tidak punya internet): font Outfit dipakai bila terpasang, selain itu sistem.
#pragma once
#include <pgmspace.h>

static const char AP_PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="id"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AirTracking · Mode AP</title>
<style>
:root{--bg:#F4F9FF;--card:#fff;--s2:#F4F9FF;--ink:#0F2A4A;--ink2:#6B84A3;--blue:#1E6FD9;--blue2:#DCEAFB;--line:#E3ECF7;--grid:#E3EEFB;--good:#1F7A4D;--good2:#DCF1E5;--warn:#9A6700;--warn2:#FBEFC8;--crit:#B3261E;--crit2:#FBDEDB;--f:'Outfit',system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;color-scheme:light}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 var(--f)}
.wrap{max-width:760px;margin:0 auto;display:flex;flex-direction:column;gap:14px;padding:16px 16px 40px}
h1,h2,h3{margin:0;letter-spacing:-.01em}h1{font-size:24px;line-height:1.1}h2{font-size:17px}h3{font-size:15px}
header{background:var(--blue);color:#fff;padding:16px 18px;border-radius:14px}
header small{opacity:.9;display:block;margin-top:4px}
.panel{background:var(--card);border:1px solid var(--line);border-radius:20px;padding:18px}
.head{display:flex;flex-wrap:wrap;justify-content:space-between;gap:8px;align-items:center;margin-bottom:10px}
label{display:block;font-size:13px;color:var(--ink2);font-weight:500}
input,select,textarea{width:100%;padding:10px 12px;margin:5px 0 10px;border:1px solid var(--line);border-radius:10px;font:inherit;background:#fff;color:var(--ink)}
input[type=checkbox]{width:auto;margin:0 6px 0 0;vertical-align:middle}
textarea{font:12.5px ui-monospace,Menlo,Consolas,monospace;min-height:84px}
.f{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:0 12px}
.btn{font:600 13.5px var(--f);padding:9px 16px;border-radius:999px;border:1px solid var(--blue);background:var(--blue);color:#fff;cursor:pointer}
.btn.ghost{background:transparent;color:var(--blue)}.btn:disabled{opacity:.6;cursor:progress}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.pill{display:inline-flex;font-size:11.5px;font-weight:600;padding:2px 9px;border-radius:999px;background:var(--blue2);color:var(--blue)}
.pill.ok{background:var(--good2);color:var(--good)}.pill.warn{background:var(--warn2);color:var(--warn)}.pill.crit{background:var(--crit2);color:var(--crit)}
.note{font-size:12.5px;color:var(--ink2);margin:8px 0 0}.err{color:var(--crit);font-size:13px;min-height:18px}.okk{color:var(--good);font-size:13px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:8px;margin-top:10px}
.tile{background:var(--s2);border-radius:12px;padding:8px 11px}.tile span{display:block;color:var(--ink2);font-size:11.5px}.tile b{font-size:20px;color:var(--blue)}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px}
.card{border:1px solid var(--line);border-radius:16px;padding:12px 14px;background:var(--s2)}
.card .head{margin-bottom:4px}
.kv{display:grid;grid-template-columns:118px 1fr;gap:6px;padding:6px 0;border-bottom:1px solid var(--grid);font-size:14px}.kv:last-child{border:0}.kv span:first-child{color:var(--ink2);font-size:12.5px}
.banner{background:var(--crit2);color:var(--crit);border-radius:12px;padding:10px 14px;font-size:13.5px;font-weight:600}
ol{margin:6px 0 0;padding-left:20px;display:flex;flex-direction:column;gap:4px;font-size:14px}
ul.nets{list-style:none;margin:0 0 10px;padding:0;display:flex;flex-direction:column;gap:6px}
ul.nets button{width:100%;text-align:left;font:inherit;color:var(--ink);background:var(--s2);border:1px solid var(--line);border-radius:10px;padding:8px 12px;cursor:pointer;display:flex;justify-content:space-between}
hr{border:0;border-top:1px solid var(--line);margin:16px 0}
code{font:12.5px ui-monospace,Menlo,Consolas,monospace;background:var(--s2);padding:1px 6px;border-radius:5px}
</style></head><body><div class="wrap">
<header><h1>AirTracking · Mode AP</h1><small id="sub">Satu halaman untuk dua ESP32: Controller dan Sensor node. Tidak ada data dikirim ke server selama mode AP.</small></header>

<div class="banner" id="lost" hidden>Koneksi ke alat terputus. Mungkin switch AP sudah dimatikan atau HP pindah WiFi. Sambungkan lagi ke WiFi <code>AirTrack-…</code>.</div>

<section class="panel"><div class="head"><h2>Dua ESP32 sekaligus</h2><span class="pill" id="pSw">Switch AP: …</span></div>
<div class="cards">
<div class="card"><div class="head"><h3>Controller · ESP32 #1</h3><span class="pill" id="pC">…</span></div><div id="stC"></div></div>
<div class="card"><div class="head"><h3>Sensor node · ESP32 #2</h3><span class="pill" id="pN">…</span></div><div id="stN"></div></div>
</div>
<div class="row" style="margin-top:12px"><button class="btn ghost" id="bSt" type="button">Segarkan status</button></div>
<p class="note">Sensor node tidak punya WiFi sendiri. Halaman ini bicara ke Controller lewat WiFi AP, lalu Controller meneruskan perintah ke Sensor node lewat kabel UART. Selama switch AP ON, Sensor node ditahan tetap bangun.</p>
</section>

<section class="panel"><div class="head"><h2>Pembacaan langsung</h2><label style="margin:0"><input type="checkbox" id="auto" checked>Segarkan otomatis (3 dtk)</label></div>
<div class="tiles" id="tiles"><div class="note">Menunggu data dari Sensor node…</div></div>
<div class="note" id="raw"></div>
<p class="note">Nilai langsung dibaca tanpa pemanasan, jadi butuh ±1 menit di larutan/air untuk stabil. Cocok untuk memantau saat kalibrasi.</p>
</section>

<section class="panel"><h2 style="margin-bottom:10px">Pengaturan Controller</h2>
<label>Tempel konfigurasi dari web (menu Setup WiFi), opsional
<textarea id="imp" placeholder='{"device_id":"AIR-01","loc":"LOK-01","proto":"http","server":"https://domain/api/ingest","interval_s":900}'></textarea></label>
<button class="btn ghost" id="bImp" type="button">Isi form dari konfigurasi</button>
<hr>
<h3 style="margin-bottom:8px">WiFi lokasi</h3>
<button class="btn ghost" id="bScan" type="button">Pindai WiFi</button>
<ul class="nets" id="nets" style="margin-top:10px"></ul>
<div class="f">
<label>SSID<input id="ssid" autocomplete="off" autocapitalize="none"></label>
<label>Kata sandi WiFi<input id="pass" type="password" autocomplete="off" placeholder="(kosongkan = tetap)"></label>
</div>
<h3 style="margin:6px 0 8px">Identitas alat</h3>
<div class="f">
<label>ID alat<input id="id" placeholder="AIR-01" autocapitalize="characters"></label>
<label>Kode lokasi<input id="loc" placeholder="LOK-01" autocapitalize="characters"></label>
<label>Credential<input id="key" type="password" autocomplete="off" placeholder="(kosongkan = tetap)"></label>
</div>
<h3 style="margin:6px 0 8px">Server</h3>
<div class="f">
<label>Protokol<select id="proto"><option value="http">HTTP (POST /api/ingest)</option><option value="mqtt">MQTT</option></select></label>
<label>Interval kirim (detik, min 60)<input id="iv" type="number" min="60" max="86400" value="900"></label>
</div>
<label>Alamat server<input id="server" placeholder="https://domain-anda/api/ingest  atau  host:1883" autocapitalize="none"></label>
<div class="err" id="err" role="alert"></div>
<div class="row"><button class="btn" id="bSave" type="button">Simpan pengaturan</button></div>
<p class="note" id="saveNote">Kata sandi WiFi dan credential hanya tersimpan di alat. Setelah menyimpan, geser switch AP ke OFF supaya alat masuk mode Internet dan langsung mengirim data.</p>
</section>

<section class="panel"><div class="head"><h2>Ukur &amp; kalibrasi Sensor node</h2></div>
<div class="row"><button class="btn" id="bMeas" type="button">Ukur penuh sekarang</button><span class="note" id="mnote" style="margin:0"></span></div>
<div class="tiles" id="mtiles"></div>
<hr>
<h3 style="margin-bottom:6px">Kalibrasi</h3>
<ol><li>Bilas probe dengan aquades, celup ke larutan standar, tunggu ±1 menit sampai pembacaan langsung stabil.</li><li>Tekan tombol yang sesuai. Ulangi untuk larutan berikutnya.</li></ol>
<div class="row" style="margin-top:12px">
<button class="btn ghost" data-cal="ph7" type="button">pH 7.00</button>
<button class="btn ghost" data-cal="ph4" type="button">pH 4.00</button>
<button class="btn ghost" data-cal="ec" type="button">EC 1413 µS/cm</button>
<button class="btn ghost" data-cal="turb" type="button">Kekeruhan 0 (aquades)</button>
<button class="btn ghost" data-cal="reset" type="button">Reset kalibrasi</button>
</div>
<div class="note" id="cnote"></div>
<p class="note">Kalibrasi pH 2 titik (7 lalu 4). Kekeruhan hanya indikatif. Hasil kalibrasi disimpan di Sensor node.</p>
</section>

<section class="panel"><h2 style="margin-bottom:6px">Selesai?</h2>
<p class="note" style="margin:0">Geser <b>switch AP ke OFF</b>. Controller menutup WiFi AP, menidurkan Sensor node, lalu langsung mengukur dan mengirim data ke server. Bila switch dibiarkan ON dan halaman tidak dipakai 30 menit, AP ditutup sendiri untuk menghemat baterai.</p>
</section>
</div>
<script>
var DEMO=/[?&]demo/.test(location.search);
var $=function(i){return document.getElementById(i)};
function esc(s){return String(s==null?'':s).replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function kv(k,v){return '<div class="kv"><span>'+k+'</span><span>'+v+'</span></div>'}
function pill(t,c){return '<span class="pill '+(c||'')+'">'+t+'</span>'}
function num(v,d){return v==null?'--':Number(v).toFixed(d)}
var TILES=[['pH','ph',2,''],['EC','ec',0,'µS/cm'],['TDS','tds',0,'ppm'],['Suhu','temp',1,'°C'],['Kekeruhan','turb',1,'NTU'],['DO','do',2,'mg/L']];
function tiles(r){return TILES.map(function(x){return '<div class="tile"><span>'+x[0]+'</span><b>'+num(r[x[1]],x[2])+'</b> <small>'+x[3]+'</small></div>'}).join('')}

/* ---------- jaringan ---------- */
function api(path,body){
 if(DEMO)return demo(path,body);
 var o=body?{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body).toString()}:{};
 return fetch(path,o).then(function(r){return r.json()})}
var busy=0,fails=0,nodeOnline=null;
function hold(p){busy++;return p.then(function(r){busy--;return r},function(e){busy--;throw e})}

/* ---------- status dua ESP ---------- */
function status(){api('/api/status').then(function(s){
 $('pC').className='pill ok';$('pC').textContent='Online';
 $('stC').innerHTML=kv('ID alat',esc(s.id||'(belum diisi)'))+kv('Firmware',esc(s.fw))+
  kv('Jam (UTC)',s.time?esc(s.time):pill('belum valid, disinkronkan NTP','warn'))+
  kv('Baterai',esc(s.bat+'% · '+s.vbat+' V'))+
  kv('microSD',s.sd?pill('terdeteksi','ok')+' '+s.pending+' data belum terkirim':pill('tidak terdeteksi','crit'))+
  kv('Kalibrasi pH / EC',esc((s.cal_ph||'belum')+' / '+(s.cal_ec||'belum')))+
  kv('Konfigurasi',s.configured?pill('lengkap','ok'):pill('belum lengkap','warn'));
 setSw(s.sw);}).catch(function(){});
 nodeInfo()}
function nodeInfo(){api('/api/node').then(function(n){
 if(!n.ok){$('pN').className='pill crit';$('pN').textContent='Offline';$('stN').innerHTML='<div class="err">'+esc(n.err||'Tidak merespons')+'</div>';return}
 $('pN').className='pill ok';$('pN').textContent='Online';
 $('stN').innerHTML=kv('Firmware',esc(n.fw))+kv('ADS1115',n.ads?pill('terdeteksi','ok'):pill('tidak terdeteksi','crit'))+
  kv('pH 7 / pH 4',esc(num(n.v7,0)+' mV / '+num(n.v4,0)+' mV'))+kv('Faktor K EC',esc(num(n.k,3)))+kv('V0 kekeruhan',esc(num(n.v0,2)+' V'))+
  kv('Mode',n.hold?pill('ditahan bangun (mode AP)','ok'):pill('normal'))}).catch(function(){})}
function setSw(on){$('pSw').className='pill '+(on?'ok':'warn');$('pSw').textContent='Switch AP: '+(on?'ON':'OFF')}

/* ---------- pembacaan langsung (Controller + Node dalam satu panggilan) ---------- */
function poll(){
 if(busy||!$('auto').checked){return next()}
 busy++;
 api('/api/live').then(function(d){
  fails=0;$('lost').hidden=true;
  var c=d.ctrl||{},n=d.node||{};
  $('pC').className='pill ok';$('pC').textContent='Online';setSw(c.sw);
  if(n.ok){
   $('tiles').innerHTML=tiles(n);
   $('raw').textContent='mV mentah: pH '+num(n.mv_ph,0)+' · EC '+num(n.mv_ec,0)+' · kekeruhan '+num(n.mv_turb,0)+' · baterai '+num(c.vbat,2)+' V';
   $('pN').className='pill ok';$('pN').textContent='Online';
   if(nodeOnline===false)nodeInfo();
   nodeOnline=true;
  }else{
   $('pN').className='pill crit';$('pN').textContent='Offline';
   $('tiles').innerHTML='<div class="err">'+esc(n.err||'Sensor node tidak merespons')+'</div>';
   nodeOnline=false;
  }
 }).catch(function(){fails++;if(fails>=3){$('lost').hidden=false;$('pC').className='pill crit';$('pC').textContent='Terputus'}})
 .then(function(){busy--;next()})}
function next(){setTimeout(poll,3000)}

/* ---------- pengaturan ---------- */
function loadCfg(){api('/api/config').then(function(c){
 $('ssid').value=c.ssid||'';$('id').value=c.id||'';$('loc').value=c.loc||'';$('proto').value=c.proto||'http';
 $('server').value=c.server||'';$('iv').value=c.interval||900;
 $('pass').placeholder=c.has_pass?'(tersimpan, kosongkan = tetap)':'';$('key').placeholder=c.has_key?'(tersimpan, kosongkan = tetap)':'';
 }).catch(function(){})}
$('bSt').onclick=status;
$('bImp').onclick=function(){try{var o=JSON.parse($('imp').value);
 if(o.device_id)$('id').value=o.device_id;if(o.loc!=null)$('loc').value=o.loc;if(o.proto)$('proto').value=o.proto;
 if(o.server)$('server').value=o.server;if(o.interval_s)$('iv').value=o.interval_s;$('err').textContent='';
 }catch(e){$('err').textContent='JSON tidak valid'}};
$('bScan').onclick=function(){var b=this;b.disabled=true;b.textContent='Memindai…';
 hold(api('/api/scan')).then(function(l){$('nets').innerHTML=l.map(function(n){return '<li><button type="button" data-s="'+esc(n.ssid)+'"><span>'+esc(n.ssid)+'</span><span class="pill">'+n.rssi+' dBm'+(n.enc?' · 🔒':'')+'</span></button></li>'}).join('')||'<li class="note">Tidak ada WiFi ditemukan</li>';
 b.disabled=false;b.textContent='Pindai WiFi'}).catch(function(){b.disabled=false;b.textContent='Pindai WiFi'})};
$('nets').onclick=function(e){var b=e.target.closest('button');if(b){$('ssid').value=b.getAttribute('data-s');$('pass').focus()}};
$('bSave').onclick=function(){var b=this;
 if(!$('ssid').value.trim()){$('err').textContent='Isi SSID WiFi';return}
 if(!$('id').value.trim()){$('err').textContent='Isi ID alat';return}
 if(!$('server').value.trim()){$('err').textContent='Isi alamat server';return}
 $('err').textContent='';b.disabled=true;
 hold(api('/api/config',{ssid:$('ssid').value.trim(),pass:$('pass').value,id:$('id').value.trim(),loc:$('loc').value.trim(),key:$('key').value,proto:$('proto').value,server:$('server').value.trim(),interval:$('iv').value}))
 .then(function(r){b.disabled=false;
  if(!r.ok){$('err').textContent=r.err||'Gagal menyimpan';return}
  if(r.reboot){b.disabled=true;b.textContent='Tersimpan, alat restart…';$('sub').textContent='Tersimpan. Sambungkan HP kembali ke WiFi biasa.'}
  else{$('saveNote').className='okk';$('saveNote').textContent='Tersimpan. Sekarang geser switch AP ke OFF untuk memulai mode Internet.';loadCfg();status()}})
 .catch(function(){b.disabled=false;$('err').textContent='Gagal menghubungi alat'})};

/* ---------- ukur penuh & kalibrasi (lewat Controller ke Sensor node) ---------- */
$('bMeas').onclick=function(){var b=this;b.disabled=true;$('mnote').textContent='Mengukur… (±1 menit, jangan tutup halaman)';
 hold(api('/api/measure',{x:1})).then(function(r){b.disabled=false;
  if(!r.ok){$('mnote').textContent='';$('mtiles').innerHTML='<div class="err">'+esc(r.err||'Gagal mengukur')+'</div>';return}
  $('mnote').textContent='Selesai';$('mtiles').innerHTML=tiles(r)}).catch(function(){b.disabled=false;$('mnote').textContent='Gagal menghubungi alat'})};
document.querySelectorAll('[data-cal]').forEach(function(b){b.onclick=function(){
 var t=b.getAttribute('data-cal');if(t==='reset'&&!confirm('Hapus semua kalibrasi di Sensor node?'))return;
 b.disabled=true;$('cnote').className='note';$('cnote').textContent='Memproses… tunggu sebentar';
 hold(api('/api/cal',{t:t})).then(function(r){b.disabled=false;
  $('cnote').className=r.ok?'okk':'err';$('cnote').textContent=r.ok?('Berhasil'+(r.detail?' · '+r.detail:'')):(r.err||'Gagal');status()
 }).catch(function(){b.disabled=false;$('cnote').className='err';$('cnote').textContent='Gagal menghubungi alat'})}});

/* ---------- mode demo: buka halaman ini langsung di browser dengan ?demo ---------- */
function demo(p,b){return new Promise(function(res){setTimeout(function(){res(demoData(p,b))},p==='/api/measure'?1500:200)})}
function demoData(p,b){
 var r=function(a,s){return +(a+(Math.random()-.5)*s).toFixed(2)};
 var n=function(){return{ok:1,ph:r(7.1,.1),ec:r(420,6),tds:r(210,3),'do':null,temp:r(27.5,.2),turb:r(3.2,.4),mv_ph:r(1500,4),mv_ec:r(310,3),mv_turb:r(2800,10)}};
 if(p==='/api/live')return{ctrl:{id:'AIR-01',bat:87,vbat:4.02,sw:true},node:n()};
 if(p==='/api/status')return{id:'AIR-01',fw:'1.1.0',time:new Date().toISOString().slice(0,19)+'Z',bat:87,vbat:4.02,sd:true,pending:3,cal_ph:'2026-10-19',cal_ec:'',configured:true,sw:true};
 if(p==='/api/node')return{ok:1,fw:'1.1.0',ads:1,v7:1500,v4:2032.4,k:1,v0:4.2,hold:1};
 if(p==='/api/config'&&b)return{ok:true,reboot:false};
 if(p==='/api/config')return{ssid:'WiFi-Lokasi',id:'AIR-01',loc:'LOK-01',proto:'http',server:'https://contoh.id/api/ingest',interval:900,has_pass:true,has_key:true};
 if(p==='/api/scan')return[{ssid:'WiFi-Lokasi',rssi:-52,enc:true},{ssid:'Kantor',rssi:-70,enc:true}];
 if(p==='/api/measure')return n();
 if(p==='/api/cal')return{ok:1,detail:'demo'};
 return{ok:true,reboot:false}}

status();loadCfg();setTimeout(poll,600);
</script></body></html>
)rawliteral";
