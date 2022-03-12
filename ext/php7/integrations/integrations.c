#include "integrations.h"

#include "../configuration.h"
#include "../logging.h"
#include "value/value.h"
#include <hook/hook.h>
#undef INTEGRATION

#define DDTRACE_DEFERRED_INTEGRATION_LOADER(class, fname, integration_name)             \
    dd_hook_method_and_unhook_on_first_call(ZAI_STRL_VIEW(class), ZAI_STRL_VIEW(fname), \
                          ZAI_STRL_VIEW(integration_name))

#define DD_SET_UP_DEFERRED_LOADING_BY_METHOD(name, Class, fname, integration)                                \
    dd_set_up_deferred_loading_by_method(name, ZAI_STRL_VIEW(Class), ZAI_STRL_VIEW(fname), \
                                         ZAI_STRL_VIEW(integration))

#define DD_SET_UP_DEFERRED_LOADING_BY_FUNCTION(name, fname, integration)                           \
    dd_set_up_deferred_loading_by_method(name, ZAI_STRING_EMPTY, ZAI_STRL_VIEW(fname), \
                                         ZAI_STRL_VIEW(integration))

// TODO revisit: now looks at gloabl instead of request local ini
#define INTEGRATION(id, lcname)                                        \
    {                                                                  \
        .name = DDTRACE_INTEGRATION_##id,                              \
        .name_ucase = #id,                                             \
        .name_lcase = (lcname),                                        \
        .name_len = sizeof(lcname) - 1,                                \
        .is_enabled = get_global_DD_TRACE_##id##_ENABLED,                     \
        .is_analytics_enabled = get_global_DD_TRACE_##id##_ANALYTICS_ENABLED, \
        .get_sample_rate = get_global_DD_TRACE_##id##_ANALYTICS_SAMPLE_RATE,  \
    },
ddtrace_integration ddtrace_integrations[] = {DD_INTEGRATIONS};
size_t ddtrace_integrations_len = sizeof ddtrace_integrations / sizeof ddtrace_integrations[0];

// Map of lowercase strings to the ddtrace_integration equivalent
static HashTable _dd_string_to_integration_name_map;

static void _dd_add_integration_to_map(char* name, size_t name_len, ddtrace_integration* integration);

void ddtrace_integrations_mshutdown(void) { zend_hash_destroy(&_dd_string_to_integration_name_map); }

static bool dd_invoke_integration_loader_and_unhook(zend_execute_data *frame, void *auxiliary, void *dynamic) {
    (void)frame, (void)dynamic;

    zval integration;
    ZVAL_STR(&integration, auxiliary);

    zval *rv;
    ZAI_VALUE_INIT(rv);
    bool success =
            zai_symbol_call_literal(ZEND_STRL("ddtrace\\integrations\\load_deferred_integration"), &rv, 1, &integration);
    ZAI_VALUE_DTOR(rv);

    if (UNEXPECTED(!success)) {
        ddtrace_log_debugf(
                "Error loading deferred integration '%s' from DDTrace\\Integrations\\load_deferred_integration",
                Z_STRVAL(integration));
    }

    zend_string *func = frame->func->common.function_name;
    zend_string *Class = frame->func->common.scope ? frame->func->common.scope->name : NULL;
    zai_hook_remove(Class ? ZAI_STRING_EMPTY : (zai_string_view){ .ptr =  ZSTR_VAL(Class), .len = ZSTR_LEN(Class) },
                    (zai_string_view){ .ptr =  ZSTR_VAL(func), .len = ZSTR_LEN(func) },
                    0);

    return true;
}

static void dd_hook_method_and_unhook_on_first_call(zai_string_view Class, zai_string_view method, zai_string_view callback) {
    zai_hook_install(Class, method,
            dd_invoke_integration_loader_and_unhook,
            NULL,
            ZAI_HOOK_AUX(zend_string_init(callback.ptr, callback.len, 1), (void(*)(void *))zend_string_release),
            0);
}

static void dd_load_test_integrations(void) {
    char* test_deferred = getenv("_DD_LOAD_TEST_INTEGRATIONS");
    if (!test_deferred) {
        return;
    }

    DDTRACE_DEFERRED_INTEGRATION_LOADER("test", "public_static_method", "ddtrace\\test\\testsandboxedintegration");
    // DDTRACE_INTEGRATION_TRACE("test", "automaticaly_traced_method", "tracing_function", DDTRACE_DISPATCH_POSTHOOK);
}

static void dd_set_up_deferred_loading_by_method(ddtrace_integration_name name, zai_string_view Class,
                                                 zai_string_view method, zai_string_view integration) {
    if (!ddtrace_config_integration_enabled(name)) {
        return;
    }

    dd_hook_method_and_unhook_on_first_call(Class, method, integration);
}

void ddtrace_integrations_minit(void) {
    zend_hash_init(&_dd_string_to_integration_name_map, ddtrace_integrations_len, NULL, NULL, 1);

    for (size_t i = 0; i < ddtrace_integrations_len; ++i) {
        char *name = ddtrace_integrations[i].name_lcase;
        size_t name_len = ddtrace_integrations[i].name_len;
        _dd_add_integration_to_map(name, name_len, &ddtrace_integrations[i]);
    }

    dd_load_test_integrations();

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_ELASTICSEARCH, "elasticsearch\\client", "__construct",
                                         "DDTrace\\Integrations\\ElasticSearch\\V1\\ElasticSearchIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_MEMCACHED, "Memcached", "__construct",
                                         "DDTrace\\Integrations\\Memcached\\MemcachedIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_MONGODB, "mongodb\\driver\\manager", "__construct",
                                         "DDTrace\\Integrations\\MongoDB\\MongoDBIntegration");
    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_MONGODB, "mongodb\\driver\\query", "__construct",
                                         "DDTrace\\Integrations\\MongoDB\\MongoDBIntegration");
    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_MONGODB, "mongodb\\driver\\command", "__construct",
                                         "DDTrace\\Integrations\\MongoDB\\MongoDBIntegration");
    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_MONGODB, "mongodb\\driver\\bulkwrite", "__construct",
                                         "DDTrace\\Integrations\\MongoDB\\MongoDBIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_NETTE, "Nette\\Configurator", "__construct",
                                         "DDTrace\\Integrations\\Nette\\NetteIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_NETTE, "Nette\\Bootstrap\\Configurator", "__construct",
                                         "DDTrace\\Integrations\\Nette\\NetteIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_PDO, "PDO", "__construct",
                                         "DDTrace\\Integrations\\PDO\\PDOIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_PHPREDIS, "Redis", "__construct",
                                         "DDTrace\\Integrations\\PHPRedis\\PHPRedisIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_PHPREDIS, "RedisCluster", "__construct",
                                         "DDTrace\\Integrations\\PHPRedis\\PHPRedisIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_PREDIS, "Predis\\Client", "__construct",
                                         "DDTrace\\Integrations\\Predis\\PredisIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_SLIM, "Slim\\App", "__construct",
                                         "DDTrace\\Integrations\\Slim\\SlimIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_FUNCTION(DDTRACE_INTEGRATION_WORDPRESS, "wp_check_php_mysql_versions",
                                           "DDTrace\\Integrations\\WordPress\\WordPressIntegration");

    DD_SET_UP_DEFERRED_LOADING_BY_METHOD(DDTRACE_INTEGRATION_YII, "yii\\di\\Container", "__construct",
                                         "DDTrace\\Integrations\\Yii\\YiiIntegration");
}

ddtrace_integration* ddtrace_get_integration_from_string(ddtrace_string integration) {
    return zend_hash_str_find_ptr(&_dd_string_to_integration_name_map, integration.ptr, integration.len);
}

static void _dd_add_integration_to_map(char* name, size_t name_len, ddtrace_integration* integration) {
    zend_hash_str_add_ptr(&_dd_string_to_integration_name_map, name, name_len, integration);
    ZEND_ASSERT(strlen(integration->name_ucase) == name_len);
    ZEND_ASSERT(DDTRACE_LONGEST_INTEGRATION_NAME_LEN >= name_len);
}
