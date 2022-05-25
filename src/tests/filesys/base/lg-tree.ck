# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(lg-tree) begin
(lg-tree) Test 1 Done
(lg-tree) Test 2 Done
(lg-tree) end
EOF
pass;
