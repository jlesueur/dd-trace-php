--TEST--
Long running autoflush
--SKIPIF--
<?php
include __DIR__ . '/../includes/skipif_no_dev_env.inc';
if (!extension_loaded('pcntl')) die('skip: pcntl extension required');
if (!extension_loaded('curl')) die('skip: curl extension required');
?>
--ENV--
DD_TRACE_GENERATE_ROOT_SPAN=false
DD_TRACE_AUTO_FLUSH_ENABLED=true
DD_TRACE_DEBUG=1
--FILE--
<?php

require 'functions.inc';

const ITERATIONS = 2;

\DDTrace\trace_function('long_running_entry_point', function ($span) {
    $span->type = 'custom';
    $span->service = 'pcntl-testing-service';
});

\DDTrace\trace_function('call_httpbin', function ($span) {
    $span->type = 'custom';
    $span->service = 'pcntl-testing-service';
});

for ($iteration = 0; $iteration < ITERATIONS; $iteration++) {
    long_running_entry_point();
    usleep(200000);
}

function long_running_entry_point()
{
    call_httpbin();

    $forkPid = pcntl_fork();

    if ($forkPid > 0) {
        // Main
        if (ddtrace_config_trace_enabled()) {
            echo "parent is enabled\n";
        }
    } else if ($forkPid === 0) {
        // Child
        usleep(100000);
        if (ddtrace_config_trace_enabled()) {
            echo "child is enabled\n";
        }
        call_httpbin();
        exit(0);
    } else {
        error_log('Error');
        exit(-1);
    }
    call_httpbin();
}

?>
--EXPECTF--
parent is enabled
Successfully triggered flush with trace of size 3
child is enabled
Successfully triggered flush with trace of size 1
Cannot run tracing closure for long_running_entry_point(); spans out of sync
No finished traces to be sent to the agent
parent is enabled
Successfully triggered flush with trace of size 3
child is enabled
Successfully triggered flush with trace of size 1
Cannot run tracing closure for long_running_entry_point(); spans out of sync
No finished traces to be sent to the agent
No finished traces to be sent to the agent
