/*
 * dump_8800.js —— v3
 *   v2 崩在"在 send 钩子里直接调 CreateFile/WriteFile"(相当于在 ws2_32 栈上重入 Win32)。
 *   v3 改成：钩子里只做纯内存读(readByteArray)，落盘交给 setInterval 在 Frida 自己的线程里做。
 *   自检：启动时先写 16B + 1.7MB 两个测试文件（已验证这条路不崩）
 */

const DIR = "D:\\dgames\\MXD186NEW\\Mxd186Launcher\\tools\\";
const OUT = DIR + "send_dump.bin";
const LOG = [];
function log(s) { const l = `[${new Date().toISOString().slice(11, 23)}] ${s}`; LOG.push(l); console.log(l); }

// ---- 自检
try {
  const t = Memory.alloc(16); t.writeByteArray([65,66,67,68]);
  File.writeAllBytes(DIR + "_w16.bin", new Uint8Array(t.readByteArray(4)));
  const b = Memory.alloc(1766816); b.writeByteArray([1,2,3]);
  File.writeAllBytes(DIR + "_w17.bin", new Uint8Array(b.readByteArray(1766816)));
  log("自检: 4B + 1.7MB 落盘均 OK");
} catch (e) { log("自检失败: " + e); }

// ---- 待落盘队列（由钩子填，由定时器写）
const queue = [];
setInterval(function () {
  if (!queue.length) return;
  const item = queue.shift();
  try {
    File.writeAllBytes(item.path, item.data);
    log(`落盘 ${item.data.length} 字节 -> ${item.path}`);
  } catch (e) { log("落盘失败: " + e); }
}, 300);

// ---- 网络钩子
const ws = Process.findModuleByName("ws2_32.dll");
function wexp(n) { for (const e of ws.enumerateExports()) if (e.name === n) return e.address; return null; }
function wHook(n, onEnter, onLeave) {
  const a = wexp(n);
  if (!a) { log(`(没有) ws2_32!${n}`); return; }
  Interceptor.attach(a, { onEnter, onLeave });
  log(`hook ws2_32!${n}`);
}

let idx = 0;
wHook("connect", function () {
  try {
    const sa = this.context.esp.add(8).readPointer();
    const port = (sa.add(2).readU8() << 8) | sa.add(3).readU8();
    const ip = [4, 5, 6, 7].map(o => sa.add(o).readU8()).join(".");
    log(`*** connect -> ${ip}:${port}`);
  } catch (e) {}
});

wHook("send", function () {
  const sp = this.context.esp;
  const buf = sp.add(8).readPointer();
  const len = sp.add(12).toInt32();
  if (len <= 0) return;
  log(`*** send len=${len}`);
  // 分块读: 单块失败不会整包丢, 记下坏块数
  const CH = 65536;
  const out = new Uint8Array(len);
  let bad = 0, ok = 0;
  for (let off = 0; off < len; off += CH) {
    const n = Math.min(CH, len - off);
    try {
      out.set(new Uint8Array(buf.add(off).readByteArray(n)), off);
      ok += n;
    } catch (e) { bad += n; }
  }
  log(`读到 ${ok} 字节, 读不到 ${bad} 字节 (buf=0x${buf.toUInt32().toString(16)})`);
  idx++;
  queue.push({ path: DIR + "send_dump_" + idx + ".bin", data: out });
});

const st = {};
wHook("recv", function () {
  st.buf = this.context.esp.add(8).readPointer();
}, function (ret) {
  const n = ret.toInt32();
  if (n > 0) {
    try {
      const u8 = new Uint8Array(st.buf.readByteArray(n));
      log(`*** recv ${n} 字节: ${Array.from(u8).map(x => x.toString(16).padStart(2, "0")).join(" ")}`);
    } catch (e) {}
  }
});

log("准备就绪：请在 BMS 窗里点「登录」（payload 由定时器落盘, 不会卡在 socket 栈上）");

