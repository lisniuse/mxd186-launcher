#define _WINSOCKAPI_   // 必须在 windows.h 之前, 否则 winsock.h / winsock2.h 打架
#include <winsock2.h>
#include <ws2tcpip.h>

#include <tlhelp32.h>   // CreateToolhelp32Snapshot

#include "auth.h"
#include "util.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

const wchar_t* AuthCodeText(AuthCode c) {
    switch (c) {
        case AuthCode::Ok:          return L"登录成功";
        case AuthCode::NoAccount:   return L"无帐号";
        case AuthCode::BadPassword: return L"密码错误";
        case AuthCode::Banned:      return L"帐号已被封停";
        case AuthCode::Network:     return L"连接服务器失败";
        case AuthCode::Config:      return L"配置错误";
        default:                    return L"登录失败";
    }
}

const wchar_t* AuthModeText(const std::wstring& mode) {
    if (mode == L"mysql") return L"MySQL 直连";
    if (mode == L"tcp")   return L"TCP 协议";
    if (mode == L"none")  return L"免校验(调试)";
    return L"未知";
}

// ---------------------------------------------------------------- MySQL 直连
namespace {

typedef void*    MYSQL_;
typedef void*    MYSQL_RES_;
typedef char**   MYSQL_ROW_;

struct MySqlApi {
    HMODULE dll = nullptr;
    MYSQL_* (__stdcall *init)(MYSQL_*) = nullptr;
    MYSQL_* (__stdcall *real_connect)(MYSQL_*, const char*, const char*, const char*,
                                      const char*, unsigned int, const char*,
                                      unsigned long) = nullptr;
    int      (__stdcall *query)(MYSQL_*, const char*) = nullptr;
    MYSQL_RES_* (__stdcall *store_result)(MYSQL_*) = nullptr;
    unsigned long long (__stdcall *num_rows)(MYSQL_RES_*) = nullptr;
    MYSQL_ROW_ (__stdcall *fetch_row)(MYSQL_RES_*) = nullptr;
    void     (__stdcall *free_result)(MYSQL_RES_*) = nullptr;
    void     (__stdcall *close)(MYSQL_*) = nullptr;
    const char* (__stdcall *error)(MYSQL_*) = nullptr;
    unsigned long (__stdcall *escape)(MYSQL_*, char*, const char*, unsigned long) = nullptr;

    bool Load() {
        if (dll) return true;
        const wchar_t* names[] = {L"libmysql.dll", L"mysql.dll", L"libmariadb.dll"};
        // 先在 exe 目录找, 再找本机 PHPStudy/Navicat 自带的那份, 最后交给系统搜索
        const wchar_t* rel[] = {L"", L"..\\Server\\phpStudy\\MySQL\\lib\\",
                                L"..\\..\\Server\\phpStudy\\MySQL\\lib\\",
                                L"..\\Server\\phpStudy\\php\\php-7.0.12-nts\\",
                                L"..\\..\\Server\\phpStudy\\php\\php-7.0.12-nts\\"};
        std::wstring dir = ExeDir();
        for (const wchar_t* n : names) {
            for (const wchar_t* r : rel) {
                std::wstring p = JoinPath(dir, std::wstring(r) + n);
                dll = LoadLibraryW(p.c_str());
                if (dll) { LogLine(L"加载 MySQL 客户端: " + p); break; }
            }
            if (dll) break;
            dll = LoadLibraryW(n);
            if (dll) break;
        }
        if (!dll) return false;
        init         = (decltype(init))GetProcAddress(dll, "mysql_init");
        real_connect = (decltype(real_connect))GetProcAddress(dll, "mysql_real_connect");
        query        = (decltype(query))GetProcAddress(dll, "mysql_query");
        store_result = (decltype(store_result))GetProcAddress(dll, "mysql_store_result");
        num_rows     = (decltype(num_rows))GetProcAddress(dll, "mysql_num_rows");
        fetch_row    = (decltype(fetch_row))GetProcAddress(dll, "mysql_fetch_row");
        free_result  = (decltype(free_result))GetProcAddress(dll, "mysql_free_result");
        close        = (decltype(close))GetProcAddress(dll, "mysql_close");
        error        = (decltype(error))GetProcAddress(dll, "mysql_error");
        escape       = (decltype(escape))GetProcAddress(dll, "mysql_real_escape_string");
        return init && real_connect && query && close;
    }
};

MySqlApi& Mysql() {
    static MySqlApi api;
    return api;
}

// 真正干活: 打开连接 -> 跑 SQL -> 返回行数
int RunSql(MYSQL_* conn, const std::string& sql, std::string& err) {
    MySqlApi& m = Mysql();
    if (m.query(conn, sql.c_str()) != 0) {
        err = m.error ? m.error(conn) : "query failed";
        return -1;
    }
    MYSQL_RES_* res = m.store_result(conn);
    if (!res) return 0;
    int rows = (int)m.num_rows(res);
    m.free_result(res);
    return rows;
}

std::string Esc(MYSQL_* conn, const std::wstring& s) {
    MySqlApi& m = Mysql();
    std::string raw = WideToGbk(s);
    std::vector<char> out(raw.size() * 2 + 2, 0);
    if (m.escape) {
        unsigned long n = m.escape(conn, out.data(), raw.c_str(), (unsigned long)raw.size());
        return std::string(out.data(), n);
    }
    return raw;
}

AuthResult LoginMySql(const Config& cfg, const std::wstring& user, const std::wstring& pass) {
    MySqlApi& m = Mysql();
    if (!m.Load())
        return {AuthCode::Config,
                L"找不到 libmysql.dll / libmariadb.dll，把 MySQL 客户端库放到登录器目录，"
                L"或把 fig.ini 的 [bms] auth 改成 tcp"};

    MYSQL_* conn = m.init(nullptr);
    if (!conn) return {AuthCode::Internal, L"mysql_init 失败"};

    // host 留空或写 auto → 用主窗体里填的那个服务器 IP
    std::wstring hostW = cfg.dbHost;
    if (hostW.empty() || hostW == L"auto" || hostW == L"same") hostW = cfg.serverIp;
    LogLine(L"MySQL host = " + hostW + L"（来源: " +
            (cfg.dbHost.empty() || cfg.dbHost == L"auto" ? L"主窗 IP" : L"fig.ini") + L"）");
    std::string host = WideToGbk(hostW), usr = WideToGbk(cfg.dbUser),
                pwd = WideToGbk(cfg.dbPass), db = WideToGbk(cfg.dbName);
    if (!m.real_connect(conn, host.c_str(), usr.c_str(), pwd.c_str(), db.c_str(),
                        (unsigned)cfg.dbPort, nullptr, 0)) {
        std::string e = m.error ? m.error(conn) : "";
        m.close(conn);
        LogLine(Format(L"MySQL 连接失败: %s", GbkToWide(e).c_str()));
        return {AuthCode::Network, Format(L"连接 MySQL 失败：%s", GbkToWide(e).c_str())};
    }

    // 按模板拼 SQL: %s -> 账号 / 密码
    std::wstring tpl = cfg.dbSql;
    std::wstring sqlw;
    size_t at = tpl.find(L"%s");
    sqlw = tpl;
    if (at != std::wstring::npos) {
        sqlw = tpl.substr(0, at) + GbkToWide(Esc(conn, user)) + tpl.substr(at + 2);
        at = sqlw.find(L"%s");
        if (at != std::wstring::npos)
            sqlw = sqlw.substr(0, at) + GbkToWide(Esc(conn, pass)) + sqlw.substr(at + 2);
    } else {
        // 模板里没有占位符时按默认语句来
        sqlw = L"SELECT id FROM accounts WHERE name='" + GbkToWide(Esc(conn, user)) +
               L"' AND password='" + GbkToWide(Esc(conn, pass)) + L"' LIMIT 1";
    }

    std::string err;
    int rows = RunSql(conn, WideToGbk(sqlw), err);
    AuthResult r;
    if (rows < 0) {
        r = {AuthCode::Internal, Format(L"SQL 执行失败：%s", GbkToWide(err).c_str())};
    } else if (rows > 0) {
        r = {AuthCode::Ok, L"登录成功"};
    } else {
        // 区分"无帐号"和"密码错误", 和原版提示语一致
        std::wstring probe = L"SELECT id FROM accounts WHERE name='" +
                             GbkToWide(Esc(conn, user)) + L"' LIMIT 1";
        int n2 = RunSql(conn, WideToGbk(probe), err);
        r = (n2 > 0) ? AuthResult{AuthCode::BadPassword, L"密码错误"}
                     : AuthResult{AuthCode::NoAccount, L"无帐号"};
    }
    m.close(conn);
    return r;
}

// ---------------------------------------------------------------- TCP 协议
#pragma pack(push, 1)
struct LoginReq {
    uint16_t op;          // 0x0001 登录
    uint16_t len;         // 后续长度
    char     account[24]; // GBK, 0 结尾
    char     password[32];
};
struct LoginResp {
    uint16_t code;        // 0 成功 1 密码错 2 无帐号 3 封号
};
#pragma pack(pop)

bool EnsureWsa() {
    static bool ok = [] {
        WSADATA w{};
        return WSAStartup(MAKEWORD(2, 2), &w) == 0;
    }();
    return ok;
}

AuthResult LoginTcp(const Config& cfg, const std::wstring& user, const std::wstring& pass) {
    if (!EnsureWsa()) return {AuthCode::Network, L"WSAStartup 失败"};

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return {AuthCode::Network, L"创建 socket 失败"};

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)cfg.serverPort);
    if (InetPtonW(AF_INET, cfg.serverIp.c_str(), &addr.sin_addr) != 1) {
        closesocket(s);
        return {AuthCode::Config, L"服务器 IP 不合法: " + cfg.serverIp};
    }

    // 2 秒连接超时, 别让界面卡死太久
    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
    connect(s, (sockaddr*)&addr, sizeof(addr));
    fd_set wf, ef;
    FD_ZERO(&wf); FD_SET(s, &wf);
    FD_ZERO(&ef); FD_SET(s, &ef);
    timeval tv{2, 0};
    int sel = select(0, nullptr, &wf, &ef, &tv);
    if (sel <= 0 || FD_ISSET(s, &ef)) {
        closesocket(s);
        return {AuthCode::Network, Format(L"连不上 %s:%d", cfg.serverIp.c_str(), cfg.serverPort)};
    }
    nonblock = 0;
    ioctlsocket(s, FIONBIO, &nonblock);

    LoginReq req{};
    req.op = 0x0001;
    req.len = (uint16_t)(sizeof(req) - 4);
    std::string a = WideToGbk(user), p = WideToGbk(pass);
    strncpy_s(req.account, a.c_str(), _TRUNCATE);
    strncpy_s(req.password, p.c_str(), _TRUNCATE);

    if (send(s, (const char*)&req, (int)sizeof(req), 0) != (int)sizeof(req)) {
        closesocket(s);
        return {AuthCode::Network, L"发送登录包失败"};
    }

    LoginResp resp{};
    int got = recv(s, (char*)&resp, sizeof(resp), MSG_WAITALL);
    closesocket(s);
    if (got < (int)sizeof(resp)) return {AuthCode::Network, L"服务器没有应答"};

    switch (resp.code) {
        case 0: return {AuthCode::Ok, L"登录成功"};
        case 1: return {AuthCode::BadPassword, L"密码错误"};
        case 2: return {AuthCode::NoAccount, L"无帐号"};
        case 3: return {AuthCode::Banned, L"帐号已被封停"};
        default: return {AuthCode::Internal, Format(L"未知应答码 %d", (int)resp.code)};
    }
}

}  // namespace

AuthResult AuthLogin(const Config& cfg, const std::wstring& account,
                     const std::wstring& password) {
    // 原版的账号长度检查: 帐号长度为5-12位字符
    if (account.size() < 5 || account.size() > 12)
        return {AuthCode::Config, L"帐号长度为5-12位字符"};
    if (password.empty())
        return {AuthCode::Config, L"请输入密码"};

    LogLine(Format(L"开始校验: auth=%s account=%s", cfg.authMode.c_str(), account.c_str()));

    if (cfg.authMode == L"mysql") return LoginMySql(cfg, account, password);
    if (cfg.authMode == L"tcp")   return LoginTcp(cfg, account, password);
    if (cfg.authMode == L"none")  return {AuthCode::Ok, L"登录成功(免校验)"};
    return {AuthCode::Config, L"未知的 auth 方式: " + cfg.authMode};
}



namespace {

// 现场枚举本机进程，拼一段"进程名\0 + 路径\0"的环境块（原版那 1.7MB 的简化版）
std::string BuildEnvBlock() {
    std::string out;
    out.reserve(64 * 1024);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    int n = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            std::string name = WideToGbk(pe.szExeFile);
            out += name;
            out.push_back('\0');

            HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hp) {
                wchar_t buf[MAX_PATH * 2]{};
                DWORD len = (DWORD)(sizeof(buf) / sizeof(buf[0]));
                if (QueryFullProcessImageNameW(hp, 0, buf, &len)) out += WideToGbk(buf);
                CloseHandle(hp);
            }
            out.push_back('\0');
            if (++n >= 400 || out.size() > 200 * 1024) break;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

}  // namespace

// ---------------------------------------------------------------- 8800 认证
// 原版「mxd186 账号登录」点下去之后真正做的事（从 Frida 抓包得到）：
//   connect serverIp:8800
//   send    "账号\r\n密码\r\naoa\r\n"  + 一段环境/进程清单（1.7MB 缓冲区，前 ~576KB 是有内容的）
//   recv    "A\n"  = 通过
bool NotifyAuth8800(const Config& cfg, const std::wstring& account, const std::wstring& password,
                    std::wstring& reply) {
    reply.clear();
    if (!EnsureWsa()) { reply = L"WSAStartup 失败"; return false; }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { reply = L"创建 socket 失败"; return false; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)cfg.authPort8800);
    if (InetPtonW(AF_INET, cfg.serverIp.c_str(), &addr.sin_addr) != 1) {
        closesocket(s);
        reply = L"服务器 IP 不合法: " + cfg.serverIp;
        return false;
    }

    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    connect(s, (sockaddr*)&addr, sizeof(addr));
    fd_set wf, ef;
    FD_ZERO(&wf); FD_SET(s, &wf);
    FD_ZERO(&ef); FD_SET(s, &ef);
    timeval tv{2, 0};
    int sel = select(0, nullptr, &wf, &ef, &tv);
    if (sel <= 0 || FD_ISSET(s, &ef)) {
        closesocket(s);
        reply = Format(L"连不上 %s:%d", cfg.serverIp.c_str(), cfg.authPort8800);
        return false;
    }
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);

    // 报文：账号\r\n密码\r\naoa\r\n + 环境块
    std::string acc = WideToGbk(account), pwd = WideToGbk(password);
    std::string payload = acc + "\r\n" + pwd + "\r\n" + "aoa\r\n";

    // 环境块（原版是 1.7MB 的进程/窗口环境上报）。
    // 实测：服务端认证只看前三行文本，环境块内容不参与判定；但为保持报文形状，
    // 这里现场枚举本机进程生成一段（不再依赖抓包文件 mxd186_8800_blob.bin）。
    // 如果 exe 旁边确实放了 mxd186_8800_blob.bin（老版本抓包），优先用它。
    {
        std::wstring blobPath = JoinPath(ExeDir(), L"mxd186_8800_blob.bin");
        HANDLE h = CreateFileW(blobPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            std::vector<char> buf(576 * 1024);
            DWORD got = 0;
            ReadFile(h, buf.data(), (DWORD)buf.size(), &got, nullptr);
            CloseHandle(h);
            payload.append(buf.data(), got);
            LogLine(Format(L"8800: 附带抓包 blob %lu 字节", got));
        } else {
            const std::string env = BuildEnvBlock();
            payload += env;
            LogLine(Format(L"8800: 现场生成环境块 %zu 字节", env.size()));
        }
    }

    if (send(s, payload.data(), (int)payload.size(), 0) != (int)payload.size()) {
        closesocket(s);
        reply = L"发送失败";
        return false;
    }

    char rb[64] = {0};
    int got = recv(s, rb, sizeof(rb) - 1, 0);
    closesocket(s);
    if (got > 0) {
        reply.assign(rb, rb + got);
        // 打印十六进制，方便核对
        std::string hex;
        char t[8];
        for (int i = 0; i < got; ++i) { snprintf(t, sizeof(t), "%02x ", (unsigned char)rb[i]); hex += t; }
        LogLine(Format(L"8800 回包 %d 字节: %s", got, GbkToWide(hex).c_str()));
        return rb[0] == 'A';
    }
    reply = L"没有回包";
    return false;
}

