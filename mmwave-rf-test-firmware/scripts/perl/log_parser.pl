#!/usr/bin/perl
# =============================================================================
# log_parser.pl - Parse RF test firmware log files
#
# Extracts test results, calibration data, and error messages from firmware
# UART log output.  Generates structured summaries for yield tracking and
# failure analysis.
#
# Usage:
#   perl log_parser.pl --input test_log.txt --output results.csv
#   perl log_parser.pl --input test_log.txt --summary
# =============================================================================

use strict;
use warnings;
use Getopt::Long;
use File::Basename;
use POSIX qw(strftime);

# Command-line options
my $input_file  = '';
my $output_file = '';
my $show_summary = 0;
my $verbose      = 0;

GetOptions(
    'input=s'   => \$input_file,
    'output=s'  => \$output_file,
    'summary'   => \$show_summary,
    'verbose'   => \$verbose,
) or die "Usage: $0 --input <file> [--output <file>] [--summary] [--verbose]\n";

die "Error: --input file required\n" unless $input_file;
die "Error: Input file not found: $input_file\n" unless -f $input_file;

# Result storage
my @results;
my %summary = (
    total    => 0,
    pass     => 0,
    fail     => 0,
    marginal => 0,
    error    => 0,
);
my @errors;
my %cal_results;

# Parse the log file
open(my $fh, '<', $input_file) or die "Cannot open $input_file: $!\n";

my $current_test = '';
my $current_channel = 0;
my $line_num = 0;

while (my $line = <$fh>) {
    chomp $line;
    $line_num++;

    # Skip empty lines and prompts
    next if $line =~ /^\s*$/;
    next if $line =~ /^>\s*$/;

    # Parse test result lines
    # Format: TEST_NAME CH# @ FREQ GHz: VALUE UNIT [LOW, HIGH] -> RESULT
    if ($line =~ /^\s*(\w+)\s+CH(\d+)\s+@\s+([\d.]+)\s+GHz:\s+([-\d.]+)\s+(\S+)\s+\[([-\d.]+),\s*([-\d.]+)\]\s+->\s+(\w+)/) {
        my %result = (
            test_name => $1,
            channel   => $2,
            freq_ghz  => $3,
            measured  => $4,
            unit      => $5,
            lower     => $6,
            upper     => $7,
            result    => $8,
            line_num  => $line_num,
        );

        push @results, \%result;
        $summary{total}++;
        $summary{lc($result{result})}++ if exists $summary{lc($result{result})};

        if ($verbose) {
            printf "  [%s] %s CH%d: %.2f %s -> %s\n",
                $result{result}, $result{test_name}, $result{channel},
                $result{measured}, $result{unit}, $result{result};
        }
    }

    # Parse calibration results
    # Format: "Result: PASS" or "Result: FAIL"
    if ($line =~ /Calibration|Cal:/i) {
        $current_test = 'calibration';
    }
    if ($current_test eq 'calibration' && $line =~ /Result:\s+(\w+)/) {
        my $cal_name = "cal_$line_num";
        $cal_results{$cal_name} = $1;
        $current_test = '';
    }

    # Parse iteration progress
    if ($line =~ /Iter\s+(\d+):\s+.*=\s+([\d.]+)\s+dB/) {
        if ($verbose) {
            print "  Cal iteration $1: $2 dB\n";
        }
    }

    # Capture error/warning messages
    if ($line =~ /ERROR|FAULT|TIMEOUT|OVERFLOW/i) {
        push @errors, { line => $line_num, message => $line };
    }
}

close($fh);

# Output CSV if requested
if ($output_file) {
    open(my $out, '>', $output_file) or die "Cannot open $output_file: $!\n";
    print $out "Test Name,Channel,Frequency (GHz),Measured,Unit,Lower Limit,Upper Limit,Result\n";
    for my $r (@results) {
        printf $out "%s,%d,%.3f,%.3f,%s,%.3f,%.3f,%s\n",
            $r->{test_name}, $r->{channel}, $r->{freq_ghz},
            $r->{measured}, $r->{unit}, $r->{lower}, $r->{upper},
            $r->{result};
    }
    close($out);
    print "Results written to: $output_file\n";
}

# Print summary
if ($show_summary || !$output_file) {
    print "\n" . "=" x 60 . "\n";
    print "RF Test Log Summary\n";
    print "=" x 60 . "\n";
    printf "Input file : %s\n", $input_file;
    printf "Total lines: %d\n", $line_num;
    printf "Date       : %s\n", strftime("%Y-%m-%d %H:%M:%S", localtime);
    print "-" x 60 . "\n";
    printf "Total tests: %d\n", $summary{total};
    printf "  PASS     : %d\n", $summary{pass};
    printf "  FAIL     : %d\n", $summary{fail};
    printf "  MARGINAL : %d\n", $summary{marginal};
    printf "  ERROR    : %d\n", $summary{error};

    if ($summary{total} > 0) {
        printf "  Yield    : %.1f%%\n", 100.0 * $summary{pass} / $summary{total};
    }

    if (scalar @errors > 0) {
        print "\nErrors/Warnings:\n";
        for my $e (@errors) {
            printf "  Line %d: %s\n", $e->{line}, $e->{message};
        }
    }

    if (scalar keys %cal_results > 0) {
        print "\nCalibration Results:\n";
        for my $k (sort keys %cal_results) {
            printf "  %s: %s\n", $k, $cal_results{$k};
        }
    }

    print "=" x 60 . "\n";
}

exit($summary{fail} > 0 ? 1 : 0);
