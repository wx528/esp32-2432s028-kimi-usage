#pragma once

// 全部返回传入的 buf，便于直接当字符串用。所有函数保证不越界写 buf。
const char* format_thousands(long value, char* buf, int buf_size);
const char* format_countdown(long seconds, char* buf, int buf_size); // <0 视为已到期
const char* format_age(long seconds, char* buf, int buf_size);       // 数据年龄
