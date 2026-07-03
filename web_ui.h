/*
 * web_ui.h  –  Gateway Diagnostic Web Interface  v3.0
 *
 * Single-page HTML/CSS/JS application stored in ESP32 PROGMEM flash.
 * Served at GET /  by the WebServer.
 *
 * Features
 *  • Ultra-premium dark glassmorphism dashboard
 *  • FR Meter dedicated section with Modbus detail
 *  • Run All Tests button + per-module Run buttons
 *  • Live result cards with animated glassy status rings
 *  • Scrollable diagnostic log console (polls /log)
 *  • Drag-and-drop OTA firmware upload with animated progress bar
 *  • Particle/glow ambient background animations
 */

#pragma once

const char index_html[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Gateway Diagnostic · ESP32-S3</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700;800;900&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
/* ═══════════════════════════════════════════════════════════
   DESIGN TOKENS
══════════════════════════════════════════════════════════════ */
:root{
  --bg:#050810;
  --bg2:#07091a;
  --surf:rgba(255,255,255,0.03);
  --surf2:rgba(255,255,255,0.055);
  --glass:rgba(14,20,40,0.75);
  --border:rgba(255,255,255,0.07);
  --border2:rgba(255,255,255,0.13);
  --text:#e8edf5;
  --text2:#a0aec0;
  --muted:#4a5568;
  --accent:#38bdf8;
  --accent2:#818cf8;
  --accent3:#a78bfa;
  --pass:#34d399;
  --pass2:#10b981;
  --warn:#fbbf24;
  --warn2:#f59e0b;
  --fail:#f87171;
  --fail2:#ef4444;
  --pend:#64748b;
  --r:16px;
  --r2:12px;
  --shadow:0 8px 32px rgba(0,0,0,0.5);
  --glow-accent:0 0 40px rgba(56,189,248,0.18);
  --glow-pass:0 0 30px rgba(52,211,153,0.2);
}

/* ═══════════════════════════════════════════════════════════
   RESET & BASE
══════════════════════════════════════════════════════════════ */
*{box-sizing:border-box;margin:0;padding:0}
html{scroll-behavior:smooth;font-size:14px}
body{
  background:var(--bg);
  color:var(--text);
  font-family:'Inter',system-ui,sans-serif;
  min-height:100vh;
  overflow-x:hidden;
  -webkit-font-smoothing:antialiased;
}

/* ═══════════════════════════════════════════════════════════
   AMBIENT BACKGROUND  (animated mesh)
══════════════════════════════════════════════════════════════ */
.bg-mesh{
  position:fixed;inset:0;z-index:0;pointer-events:none;overflow:hidden;
}
.bg-mesh::before,.bg-mesh::after,.bg-mesh .g3,.bg-mesh .g4{
  content:'';position:absolute;border-radius:50%;
  filter:blur(80px);animation:drift 18s ease-in-out infinite;
}
.bg-mesh::before{
  width:700px;height:700px;top:-200px;left:-150px;
  background:radial-gradient(circle,rgba(56,189,248,0.07) 0,transparent 70%);
  animation-duration:20s;
}
.bg-mesh::after{
  width:600px;height:600px;bottom:-150px;right:-100px;
  background:radial-gradient(circle,rgba(129,140,248,0.06) 0,transparent 70%);
  animation-duration:24s;animation-direction:reverse;
}
.bg-mesh .g3{
  width:400px;height:400px;top:40%;left:55%;
  background:radial-gradient(circle,rgba(167,139,250,0.05) 0,transparent 70%);
  animation-duration:16s;animation-delay:-8s;
}
.bg-mesh .g4{
  width:300px;height:300px;top:20%;right:20%;
  background:radial-gradient(circle,rgba(52,211,153,0.04) 0,transparent 70%);
  animation-duration:22s;animation-delay:-4s;
}
@keyframes drift{
  0%,100%{transform:translate(0,0) scale(1)}
  33%{transform:translate(30px,-20px) scale(1.05)}
  66%{transform:translate(-20px,25px) scale(0.97)}
}

/* Floating orbs */
.orb{
  position:fixed;border-radius:50%;pointer-events:none;z-index:0;
  animation:orbFloat 12s ease-in-out infinite;
}
.orb-1{width:6px;height:6px;background:rgba(56,189,248,0.6);top:20%;left:10%;box-shadow:0 0 12px rgba(56,189,248,0.8);animation-delay:0s}
.orb-2{width:4px;height:4px;background:rgba(129,140,248,0.7);top:60%;right:15%;box-shadow:0 0 10px rgba(129,140,248,0.8);animation-delay:-4s;animation-duration:15s}
.orb-3{width:5px;height:5px;background:rgba(167,139,250,0.5);bottom:30%;left:40%;box-shadow:0 0 10px rgba(167,139,250,0.7);animation-delay:-8s;animation-duration:18s}
.orb-4{width:3px;height:3px;background:rgba(52,211,153,0.6);top:80%;left:70%;box-shadow:0 0 8px rgba(52,211,153,0.8);animation-delay:-2s;animation-duration:10s}
@keyframes orbFloat{
  0%,100%{transform:translateY(0) translateX(0);opacity:0.6}
  25%{transform:translateY(-40px) translateX(20px);opacity:1}
  50%{transform:translateY(-20px) translateX(-15px);opacity:0.8}
  75%{transform:translateY(-60px) translateX(10px);opacity:0.9}
}

/* ═══════════════════════════════════════════════════════════
   HEADER
══════════════════════════════════════════════════════════════ */
header{
  position:sticky;top:0;z-index:200;
  background:rgba(5,8,16,0.88);
  backdrop-filter:blur(28px) saturate(1.4);
  -webkit-backdrop-filter:blur(28px) saturate(1.4);
  border-bottom:1px solid var(--border);
  padding:12px 28px;
  display:flex;align-items:center;justify-content:space-between;
}
.logo{display:flex;align-items:center;gap:14px}
.logo-icon{
  width:42px;height:42px;border-radius:12px;
  background:linear-gradient(135deg,#38bdf8 0%,#818cf8 50%,#a78bfa 100%);
  display:flex;align-items:center;justify-content:center;
  font-size:22px;flex-shrink:0;
  box-shadow:0 0 20px rgba(56,189,248,0.35),0 4px 12px rgba(0,0,0,0.4);
  animation:iconPulse 3s ease-in-out infinite;
}
@keyframes iconPulse{
  0%,100%{box-shadow:0 0 20px rgba(56,189,248,0.35),0 4px 12px rgba(0,0,0,0.4)}
  50%{box-shadow:0 0 35px rgba(56,189,248,0.55),0 4px 16px rgba(0,0,0,0.5)}
}
.logo-name{font-size:15px;font-weight:800;letter-spacing:-.4px;color:var(--text)}
.logo-sub{font-size:11px;color:var(--muted);margin-top:2px;font-weight:400}
.header-right{display:flex;align-items:center;gap:10px}
.conn-pill{
  display:flex;align-items:center;gap:7px;
  padding:6px 14px;border-radius:24px;font-size:12px;font-weight:600;
  background:rgba(52,211,153,0.08);
  border:1px solid rgba(52,211,153,0.22);
  color:var(--pass);letter-spacing:.2px;
}
.conn-dot{
  width:7px;height:7px;border-radius:50%;background:var(--pass);
  box-shadow:0 0 6px var(--pass);
  animation:pulse-dot 2s ease-in-out infinite;
}
@keyframes pulse-dot{
  0%,100%{opacity:1;transform:scale(1)}
  50%{opacity:.5;transform:scale(0.7)}
}
.fw-badge{
  padding:5px 11px;border-radius:20px;font-size:11px;font-weight:600;
  background:rgba(255,255,255,0.04);border:1px solid var(--border);
  color:var(--muted);letter-spacing:.3px;
}

/* ═══════════════════════════════════════════════════════════
   MAIN LAYOUT
══════════════════════════════════════════════════════════════ */
main{max-width:960px;margin:0 auto;padding:32px 16px;position:relative;z-index:1}

/* ═══════════════════════════════════════════════════════════
   HERO
══════════════════════════════════════════════════════════════ */
.hero{text-align:center;margin:16px 0 40px}
.hero h1{
  font-size:32px;font-weight:900;letter-spacing:-1px;margin-bottom:8px;
  background:linear-gradient(135deg,#e8edf5 0%,#38bdf8 40%,#818cf8 70%,#a78bfa 100%);
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;
  background-clip:text;
  animation:heroGlow 4s ease-in-out infinite;
}
@keyframes heroGlow{
  0%,100%{filter:brightness(1)}
  50%{filter:brightness(1.15)}
}
.hero p{color:var(--text2);font-size:13px;margin-bottom:28px;font-weight:400}

.btn-all{
  padding:14px 52px;font-size:15px;font-weight:700;
  background:linear-gradient(135deg,#38bdf8,#818cf8,#a78bfa);
  background-size:200% 200%;
  color:#fff;border:none;border-radius:50px;cursor:pointer;
  letter-spacing:.3px;position:relative;overflow:hidden;
  box-shadow:0 0 40px rgba(56,189,248,0.28),0 4px 20px rgba(0,0,0,0.4);
  transition:all .3s cubic-bezier(.4,0,.2,1);
  animation:gradShift 4s ease-in-out infinite;
}
@keyframes gradShift{
  0%,100%{background-position:0% 50%}
  50%{background-position:100% 50%}
}
.btn-all::before{
  content:'';position:absolute;inset:0;
  background:linear-gradient(135deg,rgba(255,255,255,0.15),transparent);
  border-radius:inherit;
}
.btn-all:hover:not(:disabled){
  transform:translateY(-4px) scale(1.02);
  box-shadow:0 0 60px rgba(56,189,248,0.45),0 8px 32px rgba(0,0,0,0.5);
}
.btn-all:active{transform:translateY(-1px) scale(0.99)}
.btn-all:disabled{opacity:.4;cursor:not-allowed;transform:none;animation:none}
.btn-all.busy{
  background:linear-gradient(135deg,#1e293b,#0f172a);
  animation:none;
}
.spin{
  display:inline-block;width:14px;height:14px;margin-right:8px;
  border:2px solid rgba(255,255,255,0.25);border-top-color:#fff;
  border-radius:50%;animation:rot .7s linear infinite;vertical-align:middle;
}
@keyframes rot{to{transform:rotate(360deg)}}

/* ═══════════════════════════════════════════════════════════
   SECTION HEADERS
══════════════════════════════════════════════════════════════ */
.sec-title{
  font-size:11px;font-weight:700;letter-spacing:1px;
  text-transform:uppercase;color:var(--muted);
  margin-bottom:12px;padding-bottom:10px;
  border-bottom:1px solid var(--border);
  display:flex;align-items:center;justify-content:space-between;
}
.sec-title .ico{margin-right:6px;font-size:13px}

/* ═══════════════════════════════════════════════════════════
   TEST GRID
══════════════════════════════════════════════════════════════ */
.grid{
  display:grid;
  grid-template-columns:repeat(auto-fill,minmax(210px,1fr));
  gap:14px;margin-bottom:32px;
}

/* ─── CARD ────────────────────────────────────────────────── */
.card{
  background:var(--glass);
  border:1px solid var(--border);
  border-radius:var(--r);
  padding:20px 18px 16px;
  position:relative;overflow:hidden;
  backdrop-filter:blur(20px) saturate(1.2);
  -webkit-backdrop-filter:blur(20px) saturate(1.2);
  transition:border-color .35s,transform .28s cubic-bezier(.4,0,.2,1),box-shadow .28s;
  cursor:default;
}
/* Glowing top stripe */
.card::before{
  content:'';position:absolute;top:0;left:0;right:0;height:2px;
  background:var(--border);border-radius:var(--r) var(--r) 0 0;
  transition:background .4s,box-shadow .4s;
}
/* Inner glow overlay */
.card::after{
  content:'';position:absolute;inset:0;border-radius:inherit;
  background:radial-gradient(ellipse 120% 60% at 50% 0%,rgba(255,255,255,0.035) 0,transparent 70%);
  pointer-events:none;
}
.card.pass::before{background:var(--pass);box-shadow:0 0 16px rgba(52,211,153,0.5)}
.card.warn::before{background:var(--warn);box-shadow:0 0 16px rgba(251,191,36,0.5)}
.card.fail::before{background:var(--fail);box-shadow:0 0 16px rgba(248,113,113,0.5)}
.card.pending::before{
  background:linear-gradient(90deg,var(--pend),rgba(100,116,139,0.3),var(--pend));
  background-size:200% 100%;
  animation:shimStripe 2s ease-in-out infinite;
}
@keyframes shimStripe{0%{background-position:100% 0}100%{background-position:-100% 0}}

.card:hover{
  border-color:var(--border2);
  transform:translateY(-5px);
  box-shadow:0 16px 48px rgba(0,0,0,0.45),var(--glow-accent);
}
.card.pass:hover{box-shadow:0 16px 48px rgba(0,0,0,0.4),var(--glow-pass)}

/* Card inner glow on state */
.card.pass {background:rgba(16,185,129,0.04);border-color:rgba(52,211,153,0.18)}
.card.fail {background:rgba(239,68,68,0.04);border-color:rgba(248,113,113,0.18)}
.card.warn {background:rgba(245,158,11,0.04);border-color:rgba(251,191,36,0.18)}

/* ─── Card parts ──────────────────────────────────────────── */
.card-head{
  display:flex;align-items:center;justify-content:space-between;
  margin-bottom:12px;
}
.card-label{
  font-size:10px;font-weight:700;letter-spacing:1px;
  text-transform:uppercase;color:var(--muted);
}
.badge{
  padding:3px 10px;border-radius:20px;
  font-size:9.5px;font-weight:700;letter-spacing:.7px;
  transition:all .3s;
}
.badge.PASS   {background:rgba(52,211,153,0.12);color:var(--pass);border:1px solid rgba(52,211,153,0.28);box-shadow:0 0 8px rgba(52,211,153,0.2)}
.badge.WARN   {background:rgba(251,191,36,0.12);color:var(--warn);border:1px solid rgba(251,191,36,0.28);box-shadow:0 0 8px rgba(251,191,36,0.2)}
.badge.FAIL   {background:rgba(248,113,113,0.12);color:var(--fail);border:1px solid rgba(248,113,113,0.28);box-shadow:0 0 8px rgba(248,113,113,0.2)}
.badge.PENDING{background:rgba(100,116,139,0.1);color:var(--pend);border:1px solid rgba(100,116,139,0.2);animation:badgePulse 2s ease-in-out infinite}
.badge.SKIP   {background:rgba(74,85,104,0.1);color:var(--muted);border:1px solid rgba(74,85,104,0.2)}
@keyframes badgePulse{0%,100%{opacity:.7}50%{opacity:1}}

.card-icon{font-size:28px;margin-bottom:8px;display:block;line-height:1;filter:drop-shadow(0 0 6px rgba(56,189,248,0.3))}
.card-detail{
  font-size:11px;color:var(--text2);min-height:38px;
  line-height:1.6;margin-bottom:14px;word-break:break-all;
  font-family:'JetBrains Mono',monospace;font-weight:400;
}

/* ─── Run button ──────────────────────────────────────────── */
.btn-run{
  width:100%;padding:8px;font-size:11.5px;font-weight:600;
  background:rgba(255,255,255,0.04);color:var(--muted);
  border:1px solid var(--border);border-radius:10px;cursor:pointer;
  transition:all .22s cubic-bezier(.4,0,.2,1);
  letter-spacing:.4px;position:relative;overflow:hidden;
}
.btn-run::before{
  content:'';position:absolute;top:50%;left:50%;
  width:0;height:0;border-radius:50%;
  background:rgba(56,189,248,0.15);
  transform:translate(-50%,-50%);
  transition:width .4s,height .4s,opacity .4s;
  opacity:0;
}
.btn-run:hover:not(:disabled){
  background:rgba(56,189,248,0.08);
  border-color:rgba(56,189,248,0.3);
  color:var(--accent);
  transform:scale(1.02);
}
.btn-run:hover:not(:disabled)::before{width:200%;height:200%;opacity:1}
.btn-run:active:not(:disabled){transform:scale(0.98)}
.btn-run:disabled{opacity:.3;cursor:not-allowed}

/* ═══════════════════════════════════════════════════════════
   FR METER SECTION
══════════════════════════════════════════════════════════════ */
.fr-section{
  margin-bottom:28px;
}
.fr-card{
  background:var(--glass);
  border:1px solid rgba(56,189,248,0.15);
  border-radius:var(--r);
  padding:22px 24px;
  backdrop-filter:blur(20px);
  -webkit-backdrop-filter:blur(20px);
  position:relative;overflow:hidden;
  transition:all .3s;
}
.fr-card::before{
  content:'';position:absolute;top:0;left:0;right:0;height:2px;
  background:linear-gradient(90deg,#38bdf8,#818cf8,#a78bfa);
  box-shadow:0 0 20px rgba(56,189,248,0.4);
}
.fr-card::after{
  content:'';position:absolute;inset:0;
  background:radial-gradient(ellipse 80% 50% at 10% 0%,rgba(56,189,248,0.05) 0,transparent 60%);
  pointer-events:none;
}
.fr-inner{display:flex;align-items:center;justify-content:space-between;gap:20px;flex-wrap:wrap}
.fr-info{}
.fr-title{font-size:14px;font-weight:700;color:var(--text);margin-bottom:4px}
.fr-sub{font-size:11.5px;color:var(--text2);font-family:'JetBrains Mono',monospace}
.fr-params{
  display:flex;gap:10px;margin-top:10px;flex-wrap:wrap;
}
.fr-param{
  padding:4px 10px;border-radius:8px;font-size:10.5px;font-weight:600;
  background:rgba(56,189,248,0.08);border:1px solid rgba(56,189,248,0.18);
  color:var(--accent);font-family:'JetBrains Mono',monospace;
}
.fr-actions{display:flex;flex-direction:column;align-items:flex-end;gap:10px}
.fr-badge-wrap{text-align:right}
.btn-fr{
  padding:10px 28px;font-size:13px;font-weight:700;
  background:linear-gradient(135deg,rgba(56,189,248,0.15),rgba(129,140,248,0.15));
  color:var(--accent);border:1px solid rgba(56,189,248,0.3);
  border-radius:10px;cursor:pointer;
  transition:all .25s;letter-spacing:.3px;white-space:nowrap;
}
.btn-fr:hover:not(:disabled){
  background:linear-gradient(135deg,rgba(56,189,248,0.25),rgba(129,140,248,0.25));
  border-color:rgba(56,189,248,0.5);
  transform:translateY(-2px);
  box-shadow:0 6px 24px rgba(56,189,248,0.25);
}
.btn-fr:disabled{opacity:.35;cursor:not-allowed;transform:none}

/* ═══════════════════════════════════════════════════════════
   LOG CONSOLE
══════════════════════════════════════════════════════════════ */
.log-wrap{margin-bottom:28px}
.log-box{
  background:rgba(3,5,12,0.9);
  border:1px solid var(--border);
  border-radius:var(--r2);
  padding:16px 18px;
  font-family:'JetBrains Mono','Fira Code','Courier New',monospace;
  font-size:11px;line-height:1.8;height:240px;overflow-y:auto;
  color:#5a7080;white-space:pre-wrap;word-break:break-all;
  backdrop-filter:blur(12px);
  position:relative;
}
.log-box::before{
  content:'';position:absolute;top:0;left:0;right:0;
  height:32px;
  background:linear-gradient(rgba(3,5,12,0.9),transparent);
  pointer-events:none;z-index:1;border-radius:var(--r2) var(--r2) 0 0;
}
.log-box .lp{color:#34d399}.log-box .lw{color:#fbbf24}.log-box .lf{color:#f87171}
/* Custom scrollbar */
.log-box::-webkit-scrollbar{width:6px}
.log-box::-webkit-scrollbar-track{background:rgba(255,255,255,0.03)}
.log-box::-webkit-scrollbar-thumb{background:rgba(255,255,255,0.1);border-radius:3px}
.log-box::-webkit-scrollbar-thumb:hover{background:rgba(255,255,255,0.18)}

/* ─── Small buttons ───────────────────────────────────────── */
.btn-xs{
  padding:5px 12px;font-size:10.5px;font-weight:600;
  background:rgba(255,255,255,0.04);color:var(--muted);
  border:1px solid var(--border);border-radius:8px;cursor:pointer;
  transition:all .2s;letter-spacing:.3px;
}
.btn-xs:hover{background:rgba(255,255,255,0.08);color:var(--text);border-color:var(--border2)}

/* ═══════════════════════════════════════════════════════════
   OTA SECTION
══════════════════════════════════════════════════════════════ */
.ota-section{margin-bottom:36px}
.ota-glass{
  background:var(--glass);
  border:1px solid var(--border);
  border-radius:var(--r);
  padding:24px;
  backdrop-filter:blur(20px);
  -webkit-backdrop-filter:blur(20px);
  position:relative;overflow:hidden;
}
.ota-glass::after{
  content:'';position:absolute;inset:0;
  background:radial-gradient(ellipse 60% 40% at 90% 10%,rgba(129,140,248,0.06) 0,transparent 60%);
  pointer-events:none;
}

.ota-header{
  display:flex;align-items:center;gap:12px;margin-bottom:20px;
}
.ota-icon-wrap{
  width:44px;height:44px;border-radius:12px;
  background:linear-gradient(135deg,rgba(56,189,248,0.15),rgba(129,140,248,0.15));
  border:1px solid rgba(56,189,248,0.2);
  display:flex;align-items:center;justify-content:center;font-size:20px;
  flex-shrink:0;
}
.ota-header-text h3{font-size:15px;font-weight:700;color:var(--text);margin-bottom:3px}
.ota-header-text p{font-size:11.5px;color:var(--text2)}

.ota-drop{
  border:2px dashed rgba(255,255,255,0.1);
  border-radius:var(--r2);
  padding:40px 24px;
  text-align:center;cursor:pointer;
  transition:all .35s cubic-bezier(.4,0,.2,1);
  background:rgba(255,255,255,0.01);
  position:relative;overflow:hidden;
}
.ota-drop::before{
  content:'';position:absolute;inset:0;
  background:radial-gradient(ellipse at 50% 50%,rgba(56,189,248,0.04) 0,transparent 70%);
  transition:opacity .3s;opacity:0;
}
.ota-drop:hover,.ota-drop.over{
  border-color:rgba(56,189,248,0.4);
  background:rgba(56,189,248,0.04);
}
.ota-drop:hover::before,.ota-drop.over::before{opacity:1}
.ota-drop.over{
  border-color:var(--accent);
  background:rgba(56,189,248,0.08);
  transform:scale(1.01);
}
.ota-drop input[type=file]{display:none}
.drop-icon{
  font-size:40px;margin-bottom:10px;display:block;
  transition:transform .3s;
  filter:drop-shadow(0 0 8px rgba(56,189,248,0.3));
}
.ota-drop:hover .drop-icon{transform:translateY(-4px) scale(1.05)}
.drop-text{color:var(--text2);font-size:13.5px;margin-bottom:4px;font-weight:500}
.drop-hint{color:var(--muted);font-size:11px}
.ota-file{
  color:var(--accent);font-size:12.5px;font-weight:600;
  margin-top:12px;min-height:18px;
  font-family:'JetBrains Mono',monospace;
}

/* Progress */
.prog-wrap{display:none;margin-top:20px}
.prog-track{
  background:rgba(255,255,255,0.05);
  border-radius:20px;height:10px;overflow:hidden;
  border:1px solid rgba(255,255,255,0.05);
}
.prog-fill{
  height:100%;border-radius:20px;width:0;
  background:linear-gradient(90deg,#38bdf8,#818cf8,#a78bfa);
  background-size:200% 100%;
  transition:width .35s cubic-bezier(.4,0,.2,1);
  animation:progShimmer 2s linear infinite;
  box-shadow:0 0 12px rgba(56,189,248,0.5);
}
@keyframes progShimmer{
  0%{background-position:200% 0}100%{background-position:-200% 0}
}
.prog-label{font-size:11.5px;color:var(--text2);margin-top:8px;font-family:'JetBrains Mono',monospace}

/* Flash button */
.btn-flash{
  display:none;margin-top:18px;padding:12px 36px;
  font-size:13.5px;font-weight:700;
  background:linear-gradient(135deg,#38bdf8,#818cf8);
  color:#fff;border:none;border-radius:10px;cursor:pointer;
  transition:all .25s;letter-spacing:.3px;
  box-shadow:0 0 30px rgba(56,189,248,0.25);
}
.btn-flash:hover{
  transform:translateY(-3px);
  box-shadow:0 0 50px rgba(56,189,248,0.45),0 6px 24px rgba(0,0,0,0.4);
}
.btn-flash:disabled{opacity:.4;cursor:not-allowed;transform:none}
.ota-status{display:none;margin-top:14px;font-size:13px;font-weight:600}

/* OTA steps indicator */
.ota-steps{
  display:flex;gap:0;margin-top:20px;
}
.ota-step{
  flex:1;text-align:center;padding:10px 8px;position:relative;
  font-size:10.5px;color:var(--muted);font-weight:600;letter-spacing:.3px;
}
.ota-step::after{
  content:'';position:absolute;bottom:0;left:10%;right:10%;height:2px;
  background:rgba(255,255,255,0.06);border-radius:2px;
  transition:background .4s;
}
.ota-step.active{color:var(--accent)}
.ota-step.active::after{background:var(--accent);box-shadow:0 0 8px rgba(56,189,248,0.4)}
.ota-step.done{color:var(--pass)}
.ota-step.done::after{background:var(--pass)}
.ota-step-icon{font-size:16px;display:block;margin-bottom:4px}

/* ═══════════════════════════════════════════════════════════
   FOOTER
══════════════════════════════════════════════════════════════ */
footer{
  text-align:center;padding:24px 16px;color:var(--muted);font-size:11px;
  border-top:1px solid var(--border);margin-top:8px;
  background:rgba(5,8,16,0.5);
}
footer a{color:var(--accent);text-decoration:none}
footer a:hover{text-decoration:underline}
.footer-grid{display:flex;justify-content:center;gap:24px;flex-wrap:wrap}
.footer-item{display:flex;align-items:center;gap:6px}
.footer-item .dot{width:4px;height:4px;border-radius:50%;background:var(--muted)}

/* ═══════════════════════════════════════════════════════════
   STATS BAR
══════════════════════════════════════════════════════════════ */
.stats-bar{
  display:flex;gap:10px;margin-bottom:28px;flex-wrap:wrap;
}
.stat-chip{
  flex:1;min-width:120px;
  background:var(--glass);border:1px solid var(--border);
  border-radius:12px;padding:14px 16px;
  backdrop-filter:blur(16px);
  display:flex;align-items:center;gap:12px;
  transition:all .25s;
}
.stat-chip:hover{border-color:var(--border2);transform:translateY(-2px)}
.stat-chip-icon{font-size:20px;flex-shrink:0;filter:drop-shadow(0 0 5px rgba(56,189,248,0.3))}
.stat-chip-val{font-size:15px;font-weight:700;color:var(--text);font-family:'JetBrains Mono',monospace}
.stat-chip-label{font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:.5px;margin-top:1px}

/* ═══════════════════════════════════════════════════════════
   SWITCH PANEL
══════════════════════════════════════════════════════════════ */
.sw-section{margin-bottom:28px}
.sw-glass{
  background:var(--glass);
  border:1px solid rgba(167,139,250,0.15);
  border-radius:var(--r);
  padding:22px 24px;
  backdrop-filter:blur(20px);
  -webkit-backdrop-filter:blur(20px);
  position:relative;overflow:hidden;
  transition:all .3s;
}
.sw-glass::before{
  content:'';position:absolute;top:0;left:0;right:0;height:2px;
  background:linear-gradient(90deg,#a78bfa,#818cf8,#38bdf8);
  box-shadow:0 0 20px rgba(167,139,250,0.45);
}
.sw-glass::after{
  content:'';position:absolute;inset:0;
  background:radial-gradient(ellipse 70% 50% at 90% 10%,rgba(167,139,250,0.05) 0,transparent 60%);
  pointer-events:none;
}
.sw-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:20px;flex-wrap:wrap;gap:12px}
.sw-header-left{display:flex;align-items:center;gap:14px}
.sw-icon-wrap{
  width:44px;height:44px;border-radius:12px;
  background:linear-gradient(135deg,rgba(167,139,250,0.18),rgba(129,140,248,0.18));
  border:1px solid rgba(167,139,250,0.25);
  display:flex;align-items:center;justify-content:center;font-size:22px;flex-shrink:0;
}
.sw-title{font-size:14px;font-weight:700;color:var(--text);margin-bottom:3px}
.sw-sub{font-size:11px;color:var(--text2)}

/* Toggle grid */
.sw-grid{
  display:grid;
  grid-template-columns:repeat(auto-fill,minmax(150px,1fr));
  gap:12px;margin-bottom:18px;
}
.sw-toggle{
  background:rgba(255,255,255,0.03);
  border:1px solid rgba(255,255,255,0.07);
  border-radius:14px;padding:16px 14px;
  display:flex;flex-direction:column;align-items:center;gap:10px;
  position:relative;overflow:hidden;
  transition:all .35s cubic-bezier(.4,0,.2,1);
  cursor:default;
}
.sw-toggle::before{
  content:'';position:absolute;top:0;left:0;right:0;height:2px;
  background:rgba(255,255,255,0.07);
  transition:background .4s,box-shadow .4s;
}
/* ON state */
.sw-toggle.on{
  background:rgba(52,211,153,0.06);
  border-color:rgba(52,211,153,0.25);
  box-shadow:0 0 24px rgba(52,211,153,0.12),inset 0 0 20px rgba(52,211,153,0.04);
}
.sw-toggle.on::before{background:var(--pass);box-shadow:0 0 12px rgba(52,211,153,0.6)}
/* OFF state */
.sw-toggle.off{
  background:rgba(248,113,113,0.04);
  border-color:rgba(248,113,113,0.12);
}
.sw-toggle.off::before{background:rgba(100,116,139,0.4)}

/* Pill indicator */
.sw-pill{
  width:52px;height:28px;border-radius:14px;
  position:relative;transition:all .35s;
  background:rgba(255,255,255,0.06);
  border:1px solid rgba(255,255,255,0.1);
  flex-shrink:0;
}
.sw-pill::after{
  content:'';position:absolute;
  width:20px;height:20px;border-radius:50%;
  top:3px;left:4px;
  background:#4a5568;
  transition:all .35s cubic-bezier(.4,0,.2,1);
  box-shadow:0 2px 6px rgba(0,0,0,0.4);
}
.sw-toggle.on .sw-pill{
  background:rgba(52,211,153,0.25);
  border-color:rgba(52,211,153,0.4);
  box-shadow:0 0 10px rgba(52,211,153,0.4);
}
.sw-toggle.on .sw-pill::after{
  left:28px;
  background:var(--pass);
  box-shadow:0 2px 8px rgba(52,211,153,0.6);
}
.sw-toggle.off .sw-pill::after{background:#374151}

/* Label & state text */
.sw-label{font-size:11px;font-weight:700;letter-spacing:.6px;color:var(--text2);text-transform:uppercase}
.sw-gpio{font-size:9.5px;color:var(--muted);font-family:'JetBrains Mono',monospace;margin-top:1px}
.sw-state{
  font-size:10px;font-weight:700;letter-spacing:.5px;
  padding:2px 8px;border-radius:8px;
  transition:all .3s;
}
.sw-toggle.on  .sw-state{color:var(--pass);background:rgba(52,211,153,0.12);border:1px solid rgba(52,211,153,0.25)}
.sw-toggle.off .sw-state{color:var(--muted);background:rgba(100,116,139,0.08);border:1px solid rgba(100,116,139,0.15)}

/* Pulse ring on ON */
.sw-ring{
  position:absolute;inset:0;border-radius:14px;
  border:1px solid transparent;
  transition:all .4s;
  pointer-events:none;
}
.sw-toggle.on .sw-ring{
  border-color:rgba(52,211,153,0.2);
  animation:swRingPulse 2s ease-in-out infinite;
}
@keyframes swRingPulse{
  0%,100%{box-shadow:0 0 0 0 rgba(52,211,153,0.15)}
  50%{box-shadow:0 0 0 6px rgba(52,211,153,0)}
}

.btn-sw{
  padding:10px 28px;font-size:13px;font-weight:700;
  background:linear-gradient(135deg,rgba(167,139,250,0.15),rgba(129,140,248,0.15));
  color:var(--accent3);border:1px solid rgba(167,139,250,0.3);
  border-radius:10px;cursor:pointer;
  transition:all .25s;letter-spacing:.3px;white-space:nowrap;
}
.btn-sw:hover:not(:disabled){
  background:linear-gradient(135deg,rgba(167,139,250,0.28),rgba(129,140,248,0.28));
  border-color:rgba(167,139,250,0.55);
  transform:translateY(-2px);
  box-shadow:0 6px 24px rgba(167,139,250,0.25);
}
.btn-sw:disabled{opacity:.35;cursor:not-allowed;transform:none}
.sw-live-dot{
  width:6px;height:6px;border-radius:50%;
  background:var(--accent3);display:inline-block;
  box-shadow:0 0 6px var(--accent3);
  animation:pulse-dot 1.5s ease-in-out infinite;
  margin-right:6px;
}

/* ═══════════════════════════════════════════════════════════
   RESPONSIVE
══════════════════════════════════════════════════════════════ */
@media(max-width:560px){
  .hero h1{font-size:24px}
  .btn-all{padding:12px 36px;font-size:14px}
  main{padding:20px 12px}
  header{padding:10px 16px}
  .stats-bar{gap:8px}
  .fr-inner{flex-direction:column;align-items:flex-start}
  .fr-actions{align-items:flex-start;width:100%}
  .btn-fr{width:100%;text-align:center}
  .sw-grid{grid-template-columns:repeat(2,1fr)}
  .sw-header{flex-direction:column;align-items:flex-start}
  .btn-sw{width:100%}
}
</style>
</head>
<body>
<div class="bg-mesh"><div class="g3"></div><div class="g4"></div></div>
<div class="orb orb-1"></div>
<div class="orb orb-2"></div>
<div class="orb orb-3"></div>
<div class="orb orb-4"></div>

<!-- ══ HEADER ══════════════════════════════════════════════ -->
<header>
  <div class="logo">
    <div class="logo-icon">⚡</div>
    <div>
      <div class="logo-name">Gateway Diagnostic</div>
      <div class="logo-sub">ESP32-S3 &nbsp;·&nbsp; 192.168.4.1 &nbsp;·&nbsp; v3.0</div>
    </div>
  </div>
  <div class="header-right">
    <div class="fw-badge" id="fw-badge">FW 3.0</div>
    <div class="conn-pill"><span class="conn-dot"></span>Connected</div>
  </div>
</header>

<main>

  <!-- ══ HERO ══════════════════════════════════════════════ -->
  <div class="hero">
    <h1>Hardware Diagnostic Suite</h1>
    <p id="last-run">Tests not yet run — press the button to begin</p>
    <button class="btn-all" id="btn-all" onclick="runAll()">&#9654;&nbsp; Run All Tests</button>
  </div>

  <!-- ══ STATS BAR ═════════════════════════════════════════ -->
  <div class="stats-bar">
    <div class="stat-chip">
      <div class="stat-chip-icon">🧠</div>
      <div>
        <div class="stat-chip-val" id="stat-heap">—</div>
        <div class="stat-chip-label">Free Heap</div>
      </div>
    </div>
    <div class="stat-chip">
      <div class="stat-chip-icon">💾</div>
      <div>
        <div class="stat-chip-val" id="stat-psram">—</div>
        <div class="stat-chip-label">Free PSRAM</div>
      </div>
    </div>
    <div class="stat-chip">
      <div class="stat-chip-icon">📡</div>
      <div>
        <div class="stat-chip-val" id="stat-clients">—</div>
        <div class="stat-chip-label">WiFi Clients</div>
      </div>
    </div>
    <div class="stat-chip">
      <div class="stat-chip-icon">✅</div>
      <div>
        <div class="stat-chip-val" id="stat-pass">0/0</div>
        <div class="stat-chip-label">Tests Passed</div>
      </div>
    </div>
  </div>

  <!-- ══ TEST CARDS ════════════════════════════════════════ -->
  <div class="sec-title"><span><span class="ico">🔬</span>Module Tests</span><span id="test-count" style="font-size:11px;color:var(--muted)"></span></div>
  <div class="grid" id="grid"></div>

  <!-- ══ SWITCH PANEL ══════════════════════════════════════ -->
  <div class="sw-section">
    <div class="sec-title">
      <span><span class="ico">🔀</span>Switch Inputs · Live Monitor</span>
      <span><span class="sw-live-dot"></span><span style="font-size:10px;color:var(--muted)">Live</span></span>
    </div>
    <div class="sw-glass">
      <div class="sw-header">
        <div class="sw-header-left">
          <div class="sw-icon-wrap">🔀</div>
          <div>
            <div class="sw-title">Physical Switch Inputs</div>
            <div class="sw-sub">GPIO reads with INPUT_PULLUP · LOW = switch closed (ON)</div>
          </div>
        </div>
        <div style="display:flex;align-items:center;gap:10px">
          <span class="badge PENDING" id="badge-switch">PENDING</span>
          <button class="btn-sw" id="btn-sw" onclick="runOne('switch')">🔀 Test Switches</button>
        </div>
      </div>
      <!-- Toggle indicators built by JS -->
      <div class="sw-grid" id="sw-grid"></div>
      <div id="sw-detail" style="font-size:11px;color:var(--muted);font-family:'JetBrains Mono',monospace;min-height:16px"></div>
    </div>
  </div>

  <!-- ══ FR METER SECTION ══════════════════════════════════ -->
  <div class="fr-section">
    <div class="sec-title"><span><span class="ico">⚡</span>FR Meter · Modbus RTU</span></div>
    <div class="fr-card">
      <div class="fr-inner">
        <div class="fr-info">
          <div class="fr-title">FR Meter — Frequency / Register Read</div>
          <div class="fr-sub" id="fr-detail">Reads Modbus holding registers from FR slave device via RS232</div>
          <div class="fr-params">
            <span class="fr-param">Slave ID: 1</span>
            <span class="fr-param">Reg: 0–1</span>
            <span class="fr-param">FC03</span>
            <span class="fr-param">9600 baud</span>
          </div>
        </div>
        <div class="fr-actions">
          <div class="fr-badge-wrap">
            <span class="badge PENDING" id="badge-fr">PENDING</span>
          </div>
          <button class="btn-fr" id="btn-fr" onclick="runOne('fr')">⚡ Run FR Test</button>
        </div>
      </div>
    </div>
  </div>

  <!-- ══ LOG CONSOLE ═══════════════════════════════════════ -->
  <div class="log-wrap">
    <div class="sec-title">
      <span><span class="ico">📋</span>Diagnostic Log</span>
      <span>
        <button class="btn-xs" onclick="fetchLog()">↻ Refresh</button>
        &nbsp;
        <button class="btn-xs" onclick="clearLog()">✕ Clear</button>
      </span>
    </div>
    <div class="log-box" id="log">Waiting for test output...</div>
  </div>

  <!-- ══ OTA FIRMWARE UPDATE ════════════════════════════════ -->
  <div class="ota-section">
    <div class="sec-title"><span><span class="ico">🔄</span>OTA Firmware Update</span></div>
    <div class="ota-glass">
      <div class="ota-header">
        <div class="ota-icon-wrap">📦</div>
        <div class="ota-header-text">
          <h3>Over-the-Air Firmware Flash</h3>
          <p>Drop a compiled <code>.bin</code> file — device reboots automatically after flash</p>
        </div>
      </div>

      <!-- Steps indicator -->
      <div class="ota-steps" id="ota-steps">
        <div class="ota-step active" id="step-select">
          <span class="ota-step-icon">📁</span>Select File
        </div>
        <div class="ota-step" id="step-upload">
          <span class="ota-step-icon">⬆</span>Upload
        </div>
        <div class="ota-step" id="step-flash">
          <span class="ota-step-icon">⚡</span>Flash
        </div>
        <div class="ota-step" id="step-reboot">
          <span class="ota-step-icon">🔄</span>Reboot
        </div>
      </div>

      <div style="margin-top:20px">
        <div class="ota-drop" id="ota-drop"
             onclick="document.getElementById('ota-file').click()"
             ondragover="event.preventDefault();this.classList.add('over')"
             ondragleave="this.classList.remove('over')"
             ondrop="handleDrop(event)">
          <input type="file" id="ota-file" accept=".bin" onchange="onFilePick(this)">
          <span class="drop-icon">📥</span>
          <div class="drop-text">Drop <code>.bin</code> firmware here or <strong>click to browse</strong></div>
          <div class="drop-hint">Max size limited by ESP32 flash partition · Reboots after success</div>
          <div class="ota-file" id="ota-fname"></div>
        </div>
        <div class="prog-wrap" id="prog-wrap">
          <div class="prog-track"><div class="prog-fill" id="prog-fill"></div></div>
          <div class="prog-label" id="prog-label">Uploading…</div>
        </div>
        <button class="btn-flash" id="btn-flash" onclick="doFlash()">⬆ Flash Firmware Now</button>
        <div class="ota-status" id="ota-status"></div>
      </div>
    </div>
  </div>

</main>

<!-- ══ FOOTER ════════════════════════════════════════════ -->
<footer>
  <div class="footer-grid">
    <div class="footer-item">ESP32-S3 Gateway Diagnostic v3.0</div>
    <div class="footer-item"><div class="dot"></div>WiFi: <strong style="color:var(--text);margin-left:4px">Esp32_Channel_Network's</strong></div>
    <div class="footer-item"><div class="dot"></div>Pass: <strong style="color:var(--text);margin-left:4px">esp32</strong></div>
    <div class="footer-item"><div class="dot"></div>IP: <a href="http://192.168.4.1">192.168.4.1</a></div>
  </div>
</footer>

<script>
'use strict';

/* ══ Module definitions ══════════════════════════════════════ */
const MODS = [
  {id:'rs232',   name:'RS232',    icon:'🔌', desc:'Loopback + Modbus RTU · 5s test · Serial2'},
  {id:'rs485',   name:'RS485',    icon:'🔗', desc:'Modbus bus probe · 5s continuous · Serial2'},
  {id:'gprs',    name:'GPRS/LTE', icon:'📡', desc:'SIM modem · AT handshake · Serial1'},
  {id:'di',      name:'DI',       icon:'⚡', desc:'Digital inputs · GPIO 38–41'},
  {id:'psram',   name:'PSRAM',    icon:'💾', desc:'External PSRAM · alloc/write/read'},
  {id:'rtc',     name:'RTC',      icon:'🕐', desc:'DS1307 · I2C clock read'},
  {id:'winbond', name:'Winbond',  icon:'🗂️', desc:'SPI flash · JEDEC ID check'},
];

/* Switch channel definitions (mirrors firmware SW_LABELS / SW_PINS) */
const SW_DEFS = [
  {label:'SW1', gpio:42},
  {label:'SW2', gpio:45},
  {label:'SW3', gpio:46},
  {label:'SW4', gpio:47},
];

let pollTimer    = null;
let selectedFile = null;
let swLiveTimer  = null;

/* ══ Build cards ══════════════════════════════════════════════ */
function buildCards() {
  document.getElementById('grid').innerHTML = MODS.map(m => `
    <div class="card pending" id="card-${m.id}">
      <div class="card-head">
        <span class="card-label">${m.name}</span>
        <span class="badge PENDING" id="badge-${m.id}">PENDING</span>
      </div>
      <span class="card-icon">${m.icon}</span>
      <div class="card-detail" id="detail-${m.id}">${m.desc}</div>
      <button class="btn-run" id="btn-${m.id}" onclick="runOne('${m.id}')">▶ Run</button>
    </div>`).join('');
}

/* ══ Build switch toggles ══════════════════════════════════════ */
function buildSwitches() {
  document.getElementById('sw-grid').innerHTML = SW_DEFS.map(s => `
    <div class="sw-toggle off" id="sw-${s.label}">
      <div class="sw-ring"></div>
      <div class="sw-pill"></div>
      <div class="sw-label">${s.label}</div>
      <div class="sw-gpio">GPIO ${s.gpio}</div>
      <div class="sw-state" id="sw-state-${s.label}">OFF</div>
    </div>`).join('');
}

/* ══ Apply switch states from /switch-state ════════════════════ */
function applySwitchStates(data) {
  data.switches.forEach(s => {
    const tog   = document.getElementById('sw-' + s.label);
    const stEl  = document.getElementById('sw-state-' + s.label);
    if (!tog) return;
    tog.classList.toggle('on',  s.on);
    tog.classList.toggle('off', !s.on);
    if (stEl) stEl.textContent = s.on ? 'ON' : 'OFF';
  });
}

/* ══ Fetch live switch states ══════════════════════════════════ */
function fetchSwitchState() {
  fetch('/switch-state')
    .then(r => r.json())
    .then(data => applySwitchStates(data))
    .catch(() => {});
}

function startSwLive() {
  if (swLiveTimer) return;
  fetchSwitchState();
  swLiveTimer = setInterval(fetchSwitchState, 1000);
}

/* ══ Fetch results ════════════════════════════════════════════ */
function fetchResults() {
  fetch('/results')
    .then(r => r.json())
    .then(data => applyResults(data))
    .catch(() => {});
}

function applyResults(data) {
  const busy   = data.running;
  const allBtn = document.getElementById('btn-all');
  if (busy) {
    allBtn.disabled = true;
    allBtn.innerHTML = '<span class="spin"></span>Running…';
    allBtn.classList.add('busy');
  } else {
    allBtn.disabled = false;
    allBtn.innerHTML = '&#9654;&nbsp; Run All Tests';
    allBtn.classList.remove('busy');
  }

  let passed = 0, total = 0;

  data.tests.forEach(t => {
    const key    = t.name.toLowerCase();
    const card   = document.getElementById('card-' + key);
    const badge  = document.getElementById('badge-' + key);
    const detail = document.getElementById('detail-' + key);
    const btn    = document.getElementById('btn-' + key);

    total++;
    if (t.status === 'PASS') passed++;

    // Special handling for FR card (not in MODS grid)
    if (key === 'fr') {
      const frBadge  = document.getElementById('badge-fr');
      const frDetail = document.getElementById('fr-detail');
      const frBtn    = document.getElementById('btn-fr');
      if (frBadge)  { frBadge.className = 'badge ' + t.status; frBadge.textContent = t.status; }
      if (frDetail) frDetail.textContent = t.detail;
      if (frBtn)    frBtn.disabled = busy;
      return;
    }

    // Special handling for Switch panel
    if (key === 'switch') {
      const swBadge  = document.getElementById('badge-switch');
      const swDetail = document.getElementById('sw-detail');
      const swBtn    = document.getElementById('btn-sw');
      if (swBadge)  { swBadge.className = 'badge ' + t.status; swBadge.textContent = t.status; }
      if (swDetail) swDetail.textContent = t.detail;
      if (swBtn)    swBtn.disabled = busy;
      return;
    }

    if (!card) return;
    const st = t.status.toLowerCase();
    card.className  = 'card ' + st;
    badge.className = 'badge ' + t.status;
    badge.textContent = t.status;
    detail.textContent = t.detail;
    if (btn) btn.disabled = busy;
  });

  document.getElementById('stat-pass').textContent = passed + '/' + total;
  document.getElementById('test-count').textContent = passed + ' passed, ' + (total - passed) + ' pending/failed';

  if (!busy && pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
    document.getElementById('last-run').textContent =
      '✓ Last run: ' + new Date().toLocaleTimeString();
  }
}

/* ══ Info polling ═════════════════════════════════════════════ */
function fetchInfo() {
  fetch('/info')
    .then(r => r.json())
    .then(d => {
      const fmt = v => v >= 1048576
        ? (v/1048576).toFixed(1) + ' MB'
        : (v/1024).toFixed(0) + ' KB';
      document.getElementById('stat-heap').textContent   = fmt(d.heap);
      document.getElementById('stat-psram').textContent  = d.psram ? fmt(d.psram) : 'N/A';
      document.getElementById('stat-clients').textContent = d.clients;
      document.getElementById('fw-badge').textContent    = 'FW ' + d.fw;
    })
    .catch(() => {});
}

/* ══ Run controls ═════════════════════════════════════════════ */
function runAll() {
  fetch('/run?test=all')
    .then(() => {
      document.getElementById('last-run').textContent = '⏳ Running all tests…';
      startPoll();
    })
    .catch(() => alert('Could not reach device'));
}

function runOne(id) {
  fetch('/run?test=' + id)
    .then(() => startPoll())
    .catch(() => {});
}

function startPoll() {
  if (pollTimer) return;
  fetchResults(); fetchLog();
  pollTimer = setInterval(() => { fetchResults(); fetchLog(); }, 1800);
}

/* ══ Log ══════════════════════════════════════════════════════ */
function fetchLog() {
  fetch('/log')
    .then(r => r.text())
    .then(txt => {
      const el = document.getElementById('log');
      el.textContent = txt || 'No log output yet.';
      el.scrollTop   = el.scrollHeight;
    })
    .catch(() => {});
}
function clearLog() {
  document.getElementById('log').textContent = '';
}

/* ══ OTA ══════════════════════════════════════════════════════ */
function setOTAStep(stepId) {
  ['step-select','step-upload','step-flash','step-reboot'].forEach(id => {
    const el = document.getElementById(id);
    el.classList.remove('active','done');
  });
  const ids = ['step-select','step-upload','step-flash','step-reboot'];
  const idx = ids.indexOf(stepId);
  ids.forEach((id, i) => {
    if (i < idx)  document.getElementById(id).classList.add('done');
    if (i === idx) document.getElementById(id).classList.add('active');
  });
}

function handleDrop(e) {
  e.preventDefault();
  document.getElementById('ota-drop').classList.remove('over');
  const f = e.dataTransfer.files[0];
  if (f) useFile(f);
}

function onFilePick(input) {
  if (input.files && input.files[0]) useFile(input.files[0]);
}

function useFile(f) {
  selectedFile = f;
  const sz = f.size < 1024*1024
    ? (f.size/1024).toFixed(1)+' KB'
    : (f.size/1024/1024).toFixed(2)+' MB';
  document.getElementById('ota-fname').textContent = '📄 ' + f.name + ' (' + sz + ')';
  document.getElementById('btn-flash').style.display  = 'inline-block';
  document.getElementById('ota-status').style.display = 'none';
  document.getElementById('prog-wrap').style.display  = 'none';
  document.getElementById('prog-fill').style.width    = '0';
  setOTAStep('step-upload');
}

function doFlash() {
  if (!selectedFile) return;
  const fd = new FormData();
  fd.append('firmware', selectedFile, selectedFile.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/ota');

  document.getElementById('prog-wrap').style.display   = 'block';
  document.getElementById('btn-flash').disabled        = true;
  document.getElementById('ota-status').style.display  = 'none';
  setOTAStep('step-flash');

  xhr.upload.addEventListener('progress', e => {
    if (!e.lengthComputable) return;
    const pct = Math.round(e.loaded / e.total * 100);
    document.getElementById('prog-fill').style.width    = pct + '%';
    document.getElementById('prog-label').textContent  = 'Uploading… ' + pct + '%';
  });

  xhr.addEventListener('load', () => {
    const ok = xhr.status === 200;
    const st = document.getElementById('ota-status');
    st.style.display = 'block';
    st.style.color   = ok ? '#34d399' : '#f87171';
    st.textContent   = ok
      ? '✓ OTA successful! Device rebooting in 2 s…'
      : '✕ OTA failed: ' + xhr.responseText;
    document.getElementById('prog-label').textContent = ok ? '✓ Upload complete!' : '✕ Upload failed';
    if (ok) setOTAStep('step-reboot');
    else { document.getElementById('btn-flash').disabled = false; setOTAStep('step-upload'); }
  });

  xhr.addEventListener('error', () => {
    const st = document.getElementById('ota-status');
    st.style.display = 'block';
    st.style.color   = '#f87171';
    st.textContent   = '✕ Network error — check WiFi connection';
    document.getElementById('btn-flash').disabled = false;
    setOTAStep('step-upload');
  });

  xhr.send(fd);
}

/* ══ Init ══════════════════════════════════════════════════════ */
buildCards();
buildSwitches();
fetchResults();
fetchLog();
fetchInfo();
startSwLive();          // start live switch polling immediately
setInterval(fetchResults, 2500);
setInterval(fetchLog,     6000);
setInterval(fetchInfo,    5000);
</script>
</body>
</html>
)HTMLEOF";
