# Make warnings fatal
use warnings;
BEGIN {$SIG{__WARN__} = sub { die "Terminating test due to warning: $_[0]" } };

use Test::More tests => 3;

use_ok('Search::Xapian');

# Check that module's version is defined and has a sane value.
ok(defined($Search::Xapian::VERSION));
ok($Search::Xapian::VERSION =~ /^\d+\.\d+\.\d+\.\d+$/);
