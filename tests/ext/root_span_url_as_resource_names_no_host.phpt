--TEST--
root span with DD_TRACE_URL_AS_RESOURCE_NAMES_ENABLED (variant using SERVER_NAME)
--SKIPIF--
--ENV--
DD_TRACE_GENERATE_ROOT_SPAN=0
DD_TRACE_URL_AS_RESOURCE_NAMES_ENABLED=1
DD_TRACE_PROPAGATE_SERVICE=1
SERVER_NAME=localhost:8888
SCRIPT_NAME=/foo.php
REQUEST_URI=/foo
METHOD=GET
--GET--
foo=bar
--FILE--
<?php
DDTrace\start_span();
DDTrace\close_span();
$spans = dd_trace_serialize_closed_spans();
var_dump($spans[0]['meta']);
?>
--EXPECTF--
array(6) {
  ["system.pid"]=>
  %s
  ["http.url"]=>
  string(25) "http://localhost:8888/foo"
  ["http.method"]=>
  string(3) "GET"
  ["_dd.p.dm"]=>
  string(12) "9753902159-1"
  ["_dd.dm.service_hash"]=>
  string(10) "9753902159"
  ["http.status_code"]=>
  string(3) "200"
}
