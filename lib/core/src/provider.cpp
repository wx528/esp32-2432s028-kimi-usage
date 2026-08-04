#include "provider.h"
#include "usage_parser.h"
#include "minimax_parser.h"

const char* provider_name(Provider p) {
  return p == PROVIDER_MINIMAX ? "MINIMAX" : "KIMI";
}

const char* provider_url(Provider p) {
  return p == PROVIDER_MINIMAX
           ? "https://www.minimaxi.com/v1/token_plan/remains"
           : "https://api.kimi.com/coding/v1/usages";
}

ParseResult provider_parse(Provider p, const char* json, UsageData* out) {
  return p == PROVIDER_MINIMAX
           ? parse_minimax_json(json, out)
           : parse_usage_json(json, out);
}
