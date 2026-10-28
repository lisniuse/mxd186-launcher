/*
 * verify_handoff.js —— v4: 抓「点登录那一下」真正发包的内容
 *
 * 已确认（v3 日志）：
 *   - 没有 WriteProcessMemory / CreateRemoteThread / SetWindowsHookEx（不注入、不钩）
 *   - 没有给 MapleStory 的窗口发消息（SendMessage 目标全是它自己的 ex_ui 控件）
 *   - 点下去之后：GetProcAddress(socket/inet_addr/htons/connect/send/recv/closesocket)
 *     + RegSetValueExA(login/pawd)  →  说明它自己连了某个 IP:端口 并发了数据（瞬间连完就关）
 *
 * v4 就是把这个包的 目标地址 + 字节 打出来。
 */

const LOG = [];
function log(s) {
  const line = `[${new Date().toISOString().slice(11, 23)}] ${s}`;
  LOG.push(line);
  console.log(line);
}
function expOf(mod, fn) {
  const m = Process.findModuleByName(mod);
  if (!m) return null;
  for (const e of m.enumerateExports()) if (e.name === fn) return e.address;
  return null;
}
function hook(mod, fn, onEnter, onLeave) {
  const a = expOf(mod, fn);
  if (!a) { log(`(没有) ${mod}!${fn}`); return; }
  Interceptor.attach(a, { onEnter, onLeave });
  log(`hook ${mod}!${fn}`);
}
const ansi = p => { try { return p && !p.isNull() ? (p.readAnsiString() || "") : ""; } catch (e) { return ""; } };

function sockaddr(p) {
  try {
    const fam = p.readU16();
    if (fam !== 2) return `family=${fam}`;
    const port = (p.add(2).readU8() << 8) | p.add(3).readU8();
    const ip = [4, 5, 6, 7].map(o => p.add(o).readU8()).join(".");
    return `${ip}:${port}`;
  } catch (e) { return "?"; }
}

function dump(p, len, tag) {
  try {
    const n = Math.min(len, 256);
    const buf = p.readByteArray(n);
    const u8 = new Uint8Array(buf);
    let hex = "", asc = "";
    for (let i = 0; i < u8.length; i++) {
      hex += u8[i].toString(16).padStart(2, "0") + " ";
      asc += (u8[i] >= 32 && u8[i] < 127) ? String.fromCharCode(u8[i]) : ".";
    }
    log(`${tag} len=${len}`);
    log(`      hex: ${hex.trim()}`);
    log(`      txt: ${asc}`);
  } catch (e) { log(`${tag} <读不了>`); }
}

// ---------- 网络：这是重点 ----------
hook("ws2_32.dll", "connect", function () {
  const sp = this.context.esp;
  const sa = sp.add(8).readPointer();
  log(`*** connect -> ${sockaddr(sa)}`);
});

hook("ws2_32.dll", "send", function () {
  const sp = this.context.esp;
  const buf = sp.add(8).readPointer();
  const len = sp.add(12).toInt32();
  if (len > 0) dump(buf, len, "*** send");
});

const recvState = {};
hook("ws2_32.dll", "recv", function () {
  const sp = this.context.esp;
  recvState.buf = sp.add(8).readPointer();
  recvState.len = sp.add(12).toInt32();
}, function (retval) {
  const n = retval.toInt32();
  if (n > 0) dump(recvState.buf, n, "*** recv");
  else log(`*** recv 返回 ${n}`);
});

hook("ws2_32.dll", "WSASend", function () { log("*** WSASend"); });
hook("ws2_32.dll", "closesocket", function () { log("*** closesocket"); });

// ---------- 注册表（确认写入内容） ----------
for (const f of ["RegSetValueExA", "RegSetValueExW"]) {
  hook("advapi32.dll", f, function () {
    const sp = this.context.esp;
    const isW = f.endsWith("W");
    const nm = isW ? (sp.add(8).readPointer().readUtf16String() || "") : ansi(sp.add(8).readPointer());
    const dt = isW ? (sp.add(20).readPointer().readUtf16String() || "") : ansi(sp.add(20).readPointer());
    if (nm) log(`RegSetValueEx${isW ? "W" : "A"} ${nm} = "${dt}"`);
  });
}

// ---------- 兜底：注入/钩子类，命中才打 ----------
for (const [m, f] of [["kernel32.dll", "WriteProcessMemory"], ["kernel32.dll", "CreateRemoteThread"],
                      ["user32.dll", "SetWindowsHookExA"], ["user32.dll", "SetWindowsHookExW"],
                      ["user32.dll", "SetClipboardData"]]) {
  hook(m, f, function () { log(`*** 命中 ${m}!${f}`); });
}

log("准备就绪：请在 BMS 窗里点「登录」（重点是抓 connect/send 的字节）");
rpc.exports = { dump: () => LOG };
