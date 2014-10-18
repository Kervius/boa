#!/usr/bin/perl
use strict;
use warnings;
use Getopt::Long;

my $dump_only = 0;
my $lang = 'perl';
my $qch = '@';

GetOptions( 'l=s' => \$lang, 'd' => \$dump_only, 'Q=s' => \$qch ) or die("Error in command line arguments\n");

#warn "lang:$lang dumpo: '$dump_only' qchar: $qch";

my $Q = qr/\Q$qch\E/;

my %lang_conf = (
	perl => {
		preamble => '',
		conclusion => '',
		print_s => 'print ',
		print_e => ";\n",
		q => sub { "q{$_[0]}"; },
		nl => 'print "\n";'."\n",
		intrp => 'perl',
	},
	shell => {
		print_s => 'echo -n ',
		print_e => "\n",
		q => sub {
				my $arg = shift;
				$arg =~ s/'/'\\''/g;
				return "'" . $arg . "'";
			},
		nl => 'echo'."\n",
		intrp => '/bin/sh',
	},
);

die "unsupported lang: $lang" unless exists $lang_conf{$lang};


my ($print_s,$print_e,$q,$nl,$intrp) = @{$lang_conf{$lang}}{qw/print_s print_e q nl intrp/};
my ($preamble, $conclusion) = @{$lang_conf{$lang}}{qw/preamble conclusion/};

my $out_handle;
if ($dump_only) {
	select STDOUT;
}
else {
	open( $out_handle, '|-', $intrp ) or die;
	select $out_handle;
}

my $state = 'fl';

print $preamble if $preamble;

while(<>) {
	chomp;
	if ($state eq 'fl') {
		$state = '';
		next if /^#!/;
	}
	if ($state eq '') {
		if (/^(\s*)$Q\{(.*)/) {
			print "$1$2\n";
			$state = 'm';
		}
		elsif (/^(\s*)$Q(.*)/s) {
			print "$1$2\n";
		}
		else {
			for (split( /($Q\{.*?\})/ )) {
				if (/^$Q\{(.*?)\}$/) {
					print $print_s,$1,$print_e;
				}
				else {
					print $print_s,$q->($_),$print_e;
				}
			}
			print $nl;
		}
	}
	elsif ($state eq 'm') {
		if (/^\}$Q$/) {
			$state = '';
			print "\n";
		}
		else {
			print $_, "\n";
		}
	}
	else {
		die;
	}
}

print $conclusion if $conclusion;

