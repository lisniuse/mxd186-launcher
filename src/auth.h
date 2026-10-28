// auth.h —— 账号校验: 复刻原版三条路(内嵌 libmysql 直连 / 自研 TCP 协议 / 关掉)
#pragma once
#include "config.h"
#include <string>

enum class AuthCode {
    Ok = 0,
    NoAccount,      // 无帐号
    BadPassword,    // 密码错误
    Banned,         // 封号
    Network,        // 连不上服务器
    Config,         // 配置/依赖缺失
    Internal
};

struct AuthResult {
    AuthCode     code = AuthCode::Internal;
    std::wstring msg;
};

AuthResult AuthLogin(const Config& cfg, const std::wstring& account, const std::wstring& password);
const wchar_t* AuthCodeText(AuthCode c);

// 供 UI 显示: 当前使用的校验方式
const wchar_t* AuthModeText(const std::wstring& mode);

// 原版「mxd186 账号登录」点下去之后真正做的事：
//   TCP -> serverIp:8800
//   发  "账号\r\n密码\r\naoa\r\n"  + 一段环境/进程清单
//   收  "A\n" 表示通过
// 返回 true = 收到 'A'；reply 里带原始回包（最多 64 字节）
bool NotifyAuth8800(const Config& cfg, const std::wstring& account, const std::wstring& password,
                    std::wstring& reply);
