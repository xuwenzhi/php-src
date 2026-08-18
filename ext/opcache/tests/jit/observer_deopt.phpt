--TEST--
JIT observer: trace-exit (deopt) reports the offending variable's runtime type
--INI--
opcache.enable=1
opcache.enable_cli=1
opcache.file_update_protection=0
opcache.jit=tracing
opcache.jit_buffer_size=64M
zend_test.jit_observer.enabled=1
--EXTENSIONS--
opcache
zend_test
--FILE--
<?php
// $v is int|string, so the (int) cast's type guard deopts; the observer must
// report the actual runtime type at the taken side exit.
function mixed_sum(array $rows): int {
    $a = 0;
    foreach ($rows as $v) { $a += (int) $v; }
    return $a;
}
$rows = [];
for ($i = 0; $i < 64; $i++) { $rows[] = ($i % 2) ? (string) $i : $i; }
for ($i = 0; $i < 200000; $i++) { mixed_sum($rows); }
echo "done\n";
?>
--EXPECTREGEX--
[\s\S]*jit-observer exit func=mixed_sum var=v type=string[\s\S]*
