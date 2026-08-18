--TEST--
JIT observer: a compiled trace's side-exit table reports a megamorphic dispatch flag
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
// One call site sees 3 concrete classes -> megamorphic; the compiled trace's
// side-exit table must carry a dispatch flag for total_area().
interface Shape { public function area(): float; }
class Circ implements Shape { public function __construct(public float $r) {} public function area(): float { return 3.14 * $this->r * $this->r; } }
class Sq   implements Shape { public function __construct(public float $s) {} public function area(): float { return $this->s * $this->s; } }
class Tri  implements Shape { public function __construct(public float $b, public float $h) {} public function area(): float { return 0.5 * $this->b * $this->h; } }

function total_area(array $shapes): float {
    $t = 0.0;
    foreach ($shapes as $s) { $t += $s->area(); }
    return $t;
}
$shapes = [];
for ($i = 0; $i < 60; $i++) {
    $shapes[] = match ($i % 3) { 0 => new Circ(1.5), 1 => new Sq(2.0), default => new Tri(3.0, 4.0) };
}
for ($i = 0; $i < 200000; $i++) { total_area($shapes); }
echo "done\n";
?>
--EXPECTREGEX--
[\s\S]*jit-observer compiled func=total_area opcode=[A-Z_]+ flag=(method_call|polymorphism)[\s\S]*
