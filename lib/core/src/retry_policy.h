#pragma once

// API 拉取失败后的下次重试间隔（秒）：base 起步，每次失败翻倍，封顶 300。
// failures 为当前已连续失败次数（0 表示刚失败 1 次，用 base）。
long retry_interval_sec(long base_sec, int failures);
