#!/usr/bin/perl
# =============================================================================
# yield_analyzer.pl - Production Yield Analysis
#
# Processes multiple test result CSV files to compute yield statistics,
# identify top failing tests, and generate Pareto charts of failure modes.
#
# Usage:
#   perl yield_analyzer.pl --dir results/ --output yield_report.txt
#   perl yield_analyzer.pl --dir results/ --pareto --top 10
# =============================================================================

use strict;
use warnings;
use Getopt::Long;
use File::Find;
use POSIX qw(strftime);

my $input_dir   = '.';
my $output_file = '';
my $show_pareto = 0;
my $top_n       = 10;
my $verbose     = 0;

GetOptions(
    'dir=s'    => \$input_dir,
    'output=s' => \$output_file,
    'pareto'   => \$show_pareto,
    'top=i'    => \$top_n,
    'verbose'  => \$verbose,
) or die "Usage: $0 --dir <directory> [--output <file>] [--pareto] [--top N]\n";

die "Error: Directory not found: $input_dir\n" unless -d $input_dir;

# Collect all CSV files
my @csv_files;
find(sub {
    push @csv_files, $File::Find::name if /\.csv$/i;
}, $input_dir);

die "No CSV files found in $input_dir\n" unless @csv_files;

printf "Found %d result files in %s\n", scalar @csv_files, $input_dir;

# Aggregate statistics
my $total_devices = 0;
my $passed_devices = 0;
my %test_stats;       # test_name => { total, pass, fail }
my %failure_modes;    # "test_name:channel" => count
my @per_device_results;

for my $file (@csv_files) {
    open(my $fh, '<', $file) or do {
        warn "Cannot open $file: $!\n";
        next;
    };

    my $header = <$fh>;  # Skip header
    my $device_pass = 1;
    my $device_tests = 0;

    while (my $line = <$fh>) {
        chomp $line;
        next if $line =~ /^\s*$/;

        my @fields = split /,/, $line;
        next unless scalar @fields >= 8;

        my ($test_name, $channel, $freq, $measured, $unit, $lower, $upper, $result) = @fields;

        # Initialize stats for this test
        $test_stats{$test_name} //= { total => 0, pass => 0, fail => 0 };
        $test_stats{$test_name}{total}++;

        if ($result =~ /PASS/i) {
            $test_stats{$test_name}{pass}++;
        } else {
            $test_stats{$test_name}{fail}++;
            $device_pass = 0;
            my $mode_key = "${test_name}:CH${channel}";
            $failure_modes{$mode_key}++;
        }

        $device_tests++;
    }

    close($fh);

    if ($device_tests > 0) {
        $total_devices++;
        $passed_devices++ if $device_pass;
        push @per_device_results, {
            file   => $file,
            tests  => $device_tests,
            passed => $device_pass,
        };
    }
}

# Compute overall yield
my $overall_yield = $total_devices > 0
    ? 100.0 * $passed_devices / $total_devices
    : 0.0;

# Generate report
my @report_lines;

push @report_lines, "=" x 70;
push @report_lines, "Production Yield Analysis Report";
push @report_lines, "=" x 70;
push @report_lines, sprintf("Date         : %s", strftime("%Y-%m-%d %H:%M:%S", localtime));
push @report_lines, sprintf("Input dir    : %s", $input_dir);
push @report_lines, sprintf("Files parsed : %d", scalar @csv_files);
push @report_lines, "-" x 70;
push @report_lines, sprintf("Total devices: %d", $total_devices);
push @report_lines, sprintf("Passed       : %d", $passed_devices);
push @report_lines, sprintf("Failed       : %d", $total_devices - $passed_devices);
push @report_lines, sprintf("YIELD        : %.2f%%", $overall_yield);
push @report_lines, "";

# Per-test yield breakdown
push @report_lines, "-" x 70;
push @report_lines, "Per-Test Yield:";
push @report_lines, sprintf("%-30s %8s %8s %8s %8s", "Test Name", "Total", "Pass", "Fail", "Yield%");
push @report_lines, "-" x 70;

for my $test (sort keys %test_stats) {
    my $s = $test_stats{$test};
    my $yield = $s->{total} > 0 ? 100.0 * $s->{pass} / $s->{total} : 0;
    push @report_lines, sprintf("%-30s %8d %8d %8d %7.1f%%",
        $test, $s->{total}, $s->{pass}, $s->{fail}, $yield);
}

# Pareto of failure modes
if ($show_pareto && scalar keys %failure_modes > 0) {
    push @report_lines, "";
    push @report_lines, "-" x 70;
    push @report_lines, "Failure Mode Pareto (Top $top_n):";
    push @report_lines, sprintf("%-40s %8s %8s", "Failure Mode", "Count", "Cumul%");
    push @report_lines, "-" x 70;

    my @sorted = sort { $failure_modes{$b} <=> $failure_modes{$a} } keys %failure_modes;
    my $total_failures = 0;
    $total_failures += $_ for values %failure_modes;

    my $cumulative = 0;
    my $rank = 0;
    for my $mode (@sorted) {
        last if $rank >= $top_n;
        $cumulative += $failure_modes{$mode};
        my $pct = 100.0 * $cumulative / $total_failures;
        push @report_lines, sprintf("%-40s %8d %7.1f%%", $mode, $failure_modes{$mode}, $pct);
        $rank++;
    }
}

push @report_lines, "=" x 70;

# Output
my $report_text = join("\n", @report_lines) . "\n";

if ($output_file) {
    open(my $out, '>', $output_file) or die "Cannot write $output_file: $!\n";
    print $out $report_text;
    close($out);
    print "Yield report written to: $output_file\n";
} else {
    print $report_text;
}

exit($overall_yield < 95.0 ? 1 : 0);
