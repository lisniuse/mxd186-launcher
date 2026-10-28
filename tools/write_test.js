/*
 * write_test.js —— 单独验证"从 Frida 往磁盘写 1.7MB"到底哪种方式不崩
 * 用法: frida -n mxd186登录器 -l write_test.js
 *   会写三个文件: w_small.bin(16B) / w_big.bin(1.7MB) / w_chunk.bin(1.7MB, 分块原生写)
 */

const DIR = "D:\\dgames\\MXD186NEW\\Mxd186Launcher\\tools\\";
const LOG = [];
function log(s) { const l = `[${new Date().toISOString().slice(11, 23)}] ${s}`; LOG.push(l); console.log(l); }

const k32 = Process.findModuleByName("kernel32.dll");
function kexp(n) { for (const e of k32.enumerateExports()) if (e.name === n) return e.address; return null; }
const CreateFileA = new NativeFunction(kexp("CreateFileA"), "pointer",
  ["pointer", "uint32", "uint32", "pointer", "uint32", "uint32", "pointer"]);
const WriteFile = new NativeFunction(kexp("WriteFile"), "int",
  ["pointer", "pointer", "uint32", "pointer", "pointer"]);
const CloseHandle = new NativeFunction(kexp("CloseHandle"), "int", ["pointer"]);
const GetLastError = new NativeFunction(kexp("GetLastError"), "uint32", []);

function openWrite(path) {
  const p = Memory.allocUtf8String(path);
  const h = CreateFileA(p, 0x40000000, 3, ptr(0), 2, 0x80, ptr(0));
  if (h.toInt32() === -1) { log(`CreateFile(${path}) 失败 err=${GetLastError()}`); return null; }
  return h;
}
function writeChunk(h, bufPtr, len) {
  const written = Memory.alloc(4);
  const ok = WriteFile(h, bufPtr, len, written, ptr(0));
  if (!ok) log(`WriteFile 失败 err=${GetLastError()}`);
  return ok;
}

// ① 小数据 + File API
try {
  const small = Memory.alloc(16);
  small.writeByteArray([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
  File.writeAllBytes(DIR + "w_small.bin", new Uint8Array(small.readByteArray(16)));
  log("① File.writeAllBytes 16B OK");
} catch (e) { log("① File API 失败: " + e); }

// ② 1.7MB 一次读 + File API
try {
  const big = Memory.alloc(1766816);
  big.writeByteArray(new Array(1024).fill(0x41));   // 先写点内容
  const data = big.readByteArray(1766816);
  log(`② readByteArray(1.7MB) OK, byteLength=${data.byteLength}`);
  File.writeAllBytes(DIR + "w_big.bin", new Uint8Array(data));
  log("② File.writeAllBytes 1.7MB OK");
} catch (e) { log("② 1.7MB 失败: " + e); }

// ③ 1.7MB 分块走原生 WriteFile
try {
  const big = Memory.alloc(1766816);
  const h = openWrite(DIR + "w_chunk.bin");
  if (h) {
    let off = 0;
    const CH = 262144;
    while (off < 1766816) {
      const n = Math.min(CH, 1766816 - off);
      if (!writeChunk(h, big.add(off), n)) break;
      off += n;
    }
    CloseHandle(h);
    log(`③ 原生分块写完成, 共 ${off} 字节`);
  }
} catch (e) { log("③ 分块写失败: " + e); }

log("测试结束");
