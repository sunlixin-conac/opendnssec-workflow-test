#!/usr/bin/perl
#
# $Id$
#
# Copyright (c) 2009 Kirei AB. All rights reserved.
# Copyright (c) 2010 Nominet UK. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
######################################################################

require 5.6.0;
use warnings;
use strict;

use Data::Dumper;
use Getopt::Long;
use Pod::Usage;
use XML::XPath;
use YAML 'LoadFile';

my $progname = "ods4bind.pl";
my $version  = '$Revision: 9053 $';
$version =~ s/\$(R)evision: (\d+) \$/revision $2/;

my $config = undef;

my $fake;
my $debug;
my $online_mode;
my $offline_mode;

sub main {
    my $help;
    my $config_file = "ods4bind.yaml";

    GetOptions(
        'help|?'   => \$help,
        'fake'     => \$fake,
        'debug'    => \$debug,
        'config=s' => \$config_file,
        'online'   => \$online_mode,
        'offline'  => \$offline_mode,
    ) or pod2usage(2);
    pod2usage(1) if ($help);

    # Can't do both online and offline mode simultaneously
    pod2usage(2) if ($online_mode && $offline_mode);

    read_config($config_file);

    # TODO: Check [ sig-validity-interval number [number] ; ]

    my @zones = keys(%{ $config->{'OpenDNSSEC'}->{'zones'} });

  ZONE:
    foreach my $zone (@zones) {
        message_info("Processing %s", $zone);

        my $zconfig  = $config->{'OpenDNSSEC'}->{'zones'}->{$zone};
        my $signconf = $zconfig->{signconf};

        if ($signconf && -r $signconf) {
            my $xp = XML::XPath->new(filename => $signconf);

            message_info("Parsing signer configuration for %s", $zone);

            if (parse_signconf($zone, $xp)) {
                message_error("Skipped zone %s due to error parsing signconf",
                    $zone);
                next ZONE;
            }

            message_info("Updating keys for %s", $zone);
            unless (update_keys($zone, $xp)) {
                message_error("Skipped zone %s due to error updating keys",
                    $zone);
                next ZONE;
            }

            my $zconfig = $config->{'OpenDNSSEC'}->{'zones'}->{$zone};

            # try to auto-detect mode if mode is not explicit
            if (!$online_mode && !$offline_mode) {

                # if both input and output is defined, use offline mode
                if (   $zconfig->{input}
                    && $zconfig->{output}
                    && -r $zconfig->{input})
                {
                    message_info(
                        "Input file for zone %s found, using offline mode",
                        $zone);
                    $offline_mode = 1;
                } else {
                    message_info(
                        "Input file for zone %s not found, using online mode",
                        $zone);
                    $online_mode = 1;
                }
            }

            if ($online_mode) {
                message_info("Notifying BIND to resign %s", $zone);
                sign_zone_online($zone, $xp);
            } else {
                message_info("Starting offline signing of %s", $zone);
                sign_zone_offline($zone, $xp);
            }

        } else {
            message_error("Failed to read signer configuration for %s", $zone);
        }
    }
}

sub read_config {
    my $filename = shift;
    $config = LoadFile($filename);

    apply_default_config();
    check_config();

    die "Failed to read zonelist"
      unless ($config->{OpenDNSSEC}->{zonelist}
        && -r $config->{OpenDNSSEC}->{zonelist});

    my $xp = XML::XPath->new(filename => $config->{OpenDNSSEC}->{zonelist});

    my $zones = $xp->findnodes('/ZoneList/Zone');

    foreach my $node ($zones->get_nodelist) {
        my $z = $node->getAttribute('name');

        $config->{'OpenDNSSEC'}->{'zones'}->{$z}->{signconf} =
          $xp->findvalue('SignerConfiguration', $node)->value();

        $config->{'OpenDNSSEC'}->{'zones'}->{$z}->{input} =
          $xp->findvalue('Adapters/Input/File', $node)->value();

        $config->{'OpenDNSSEC'}->{'zones'}->{$z}->{output} =
          $xp->findvalue('Adapters/Output/File', $node)->value();
    }
}

sub apply_default_config {
    unless ($config->{'BIND'}->{'config'}) {
        $config->{'BIND'}->{'config'} = "/etc/named.conf";
    }

    unless ($config->{'BIND'}->{'keysdir'}) {
        $config->{'BIND'}->{'keysdir'} = ".";
    }

    unless ($config->{'BIND'}->{'labelprefix'}) {
        $config->{'BIND'}->{'labelprefix'} = "";
    }

    unless ($config->{'BIND'}->{'dnssec-keyfromlabel'}) {
        $config->{'BIND'}->{'dnssec-keyfromlabel'} = "dnssec-keyfromlabel";
    }

    unless ($config->{'BIND'}->{'dnssec-settime'}) {
        $config->{'BIND'}->{'dnssec-settime'} = "dnssec-settime";
    }

    unless ($config->{'BIND'}->{'dnssec-signzone'}) {
        $config->{'BIND'}->{'dnssec-signzone'} = "dnssec-signzone";
    }

    unless ($config->{'BIND'}->{'ncpu'}) {
        $config->{'BIND'}->{'ncpu'} = 1;
    }

    unless ($config->{'BIND'}->{'engine'}) {
        $config->{'BIND'}->{'engine'} = "pkcs11";
    }

    unless ($config->{'OpenDNSSEC'}->{'zonelist'}) {
        $config->{'OpenDNSSEC'}->{'zonelist'} = "/etc/opendnssec/zonelist.xml";
    }
}

sub check_config {
    return if ($fake);

    unless (-x $config->{'BIND'}->{'dnssec-keyfromlabel'}) {
        die "dnssec-keyfromlabel not found";
    }

    unless (-x $config->{'BIND'}->{'dnssec-signzone'}) {
        die "dnssec-signzone not found";
    }
}

sub parse_signconf {
    my $zone = shift;
    my $xp   = shift;

    my $zconfig = $config->{'OpenDNSSEC'}->{'zones'}->{$zone};

    # Signature Refresh
    $zconfig->{refresh} = interval2seconds(
        $xp->findvalue('/SignerConfiguration/Zone/Signatures/Refresh')->value()
    );

    # Signature Inception
    my $validity = interval2seconds(
        $xp->findvalue('/SignerConfiguration/Zone/Signatures/Validity/Default')
          ->value());
    my $inception_offset = interval2seconds(
        $xp->findvalue('/SignerConfiguration/Zone/Signatures/InceptionOffset')
          ->value());
    $zconfig->{start} = sprintf("now-%d", $inception_offset);

    # Signature Expiration
    $zconfig->{end} = sprintf("now+%d", $validity);

    # Signature Jitter
    $zconfig->{jitter} = interval2seconds(
        $xp->findvalue('/SignerConfiguration/Zone/Signatures/Jitter')->value());

    # Denial of Existence
    if ($xp->exists('/SignerConfiguration/Zone/Denial/NSEC3')) {
        if ($xp->exists('/SignerConfiguration/Zone/Denial/NSEC3/OptOut')) {
            $zconfig->{nsec3}->{optout} = 1;
        }

        my $algorithm = $xp->findvalue(
            '/SignerConfiguration/Zone/Denial/NSEC3/Hash/Algorithm')->value();

        unless ($algorithm == 1) {
            message_error("Unsupported NSEC3 Algorithm (%s)", $zone);
            return -1;
        }

        $zconfig->{nsec3}->{algorithm}  = $algorithm;
        $zconfig->{nsec3}->{iterations} = $xp->findvalue(
            '/SignerConfiguration/Zone/Denial/NSEC3/Hash/Iterations')->value();
        $zconfig->{nsec3}->{salt} =
          $xp->findvalue('/SignerConfiguration/Zone/Denial/NSEC3/Hash/Salt')
          ->value();

    } else {
        $zconfig->{nsec} = 1;
    }

    # SOA serial format
    $zconfig->{serialformat} =
      $xp->findvalue('/SignerConfiguration/Zone/SOA/Serial')->value();

    if (   $zconfig->{serialformat} ne "unixtime"
        && $zconfig->{serialformat} ne "keep")
    {
        message_error("Only serial formats unixtime and keep supported (%s)",
            $zone);
        return -1;
    }

    return 0;
}

sub interval2seconds {
    my $duration = shift;
    my $result   = 0;

    if ($duration =~
        /^P((\d+)Y)?((\d+)M)?((\d+)D)?(T((\d+)H)?((\d+)M)?((\d+)S)?)?/)
    {
        my $years   = $2  ? $2  : 0;
        my $months  = $4  ? $4  : 0;
        my $days    = $6  ? $6  : 0;
        my $hours   = $9  ? $9  : 0;
        my $minutes = $11 ? $11 : 0;
        my $seconds = $13 ? $13 : 0;

        $months  += $years * 12 + $months;
        $days    += $months * 31;
        $hours   += $days * 24;
        $minutes += $hours * 60;
        $seconds += $minutes * 60;

        $result = $seconds;
    }

    return $result;
}

sub update_keys {
    my $zone = shift;
    my $xp   = shift;

    my $keysdir = $config->{'BIND'}->{'keysdir'};
    my $tempdir = $config->{'BIND'}->{'tempdir'};

    my $nkeys = 0;

    # extract current keys into temporary directory
    my @current_base = extract_keys($zone, $xp, $tempdir);

    # if no keys where extracted, return error
    if ($#current_base < 0) {
        return 0;
    } else {
        $nkeys = $#current_base + 1;
    }

    # find previous keys
    opendir(DIR, $keysdir);
    my @previous_file =
      grep { /^K$zone\.\+\d{3}\+\d{5}\.private$/ } readdir(DIR);
    closedir(DIR);

    # create list of stale (no longer used) keys
    my @current_file = map { sprintf("%s.private", $_) } @current_base;
    my @stale_file = ();
    foreach my $p (@previous_file) {
        my $found = 0;
        foreach my $c (@current_file) {
            $found++ if ($p eq $c);
        }
        push @stale_file, $p unless ($found);
    }

    # move extracted key files to key directory
    foreach my $c (@current_base) {
        my $cmd_mv;

        $cmd_mv = sprintf("mv -f %s/%s.key %s", $tempdir, $c, $keysdir);
        message_debug("> %s", $cmd_mv);
        system($cmd_mv) unless ($fake);

        $cmd_mv = sprintf("mv -f %s/%s.private %s", $tempdir, $c, $keysdir);
        message_debug("> %s", $cmd_mv);
        system($cmd_mv) unless ($fake);
    }

    # mark stale keys as deleted
    foreach my $s (@stale_file) {
        my $cmd_settime = sprintf(
            "%s -K %s -D now %s",
            $config->{'BIND'}->{'dnssec-settime'},
            $keysdir, $s
        );
        message_debug("> %s", $cmd_settime);
        system($cmd_settime) unless ($fake);
    }

    return $nkeys;
}

sub extract_keys {
    my $zone = shift;
    my $xp   = shift;
    my $dir  = shift;

    my $zconfig  = $config->{'OpenDNSSEC'}->{'zones'}->{$zone};
    my $keys     = $xp->findnodes('/SignerConfiguration/Zone/Keys/Key');
    my @keyfiles = ();
    my $errors   = 0;

    message_debug("Extracting keys for %s", $zone);

    foreach my $node ($keys->get_nodelist) {
        my $label     = $xp->findvalue('Locator',   $node)->value();
        my $algorithm = $xp->findvalue('Algorithm', $node)->value();
        my $flags     = $xp->findvalue('Flags',     $node)->value();

        message_debug("Extracting with label %s", $label);

        my $arg_keyflag = "";
        my $arg_revoke  = "";

        # Is this a zone key? If not, skip it.
        unless ($flags & 0x0100) {
            message_error("Key %s not a zone key (%d)", $label, $flags);
            $errors++;
            next;
        }

        # Will this key sign the DNSKEY RRset?
        if ($flags & 0x0001) {
            message_debug("Key %s marked as KSK", $label);
            $arg_keyflag = sprintf("%s -f KSK", $arg_keyflag);
        } else {
            message_debug("Key %s marked as ZSK", $label);
        }

        # Is the key revoked?
        if ($flags & 0x0080) {
            message_debug("Key %s marked as REVOKE", $label);
            $arg_keyflag = sprintf("%s -f REVOKE", $arg_keyflag);
            $arg_revoke = "-R now";
        }

        # Is the key to be published?
        my $arg_publish = undef;
        if ($xp->exists('Publish', $node)) {
            message_debug("Key %s published", $label);
            $arg_publish = "-P now";
        } else {
            message_debug("Key %s not published", $label);
            $arg_publish = "-P none";
        }

        # Is the key to be used for signing?
        my $arg_active = undef;
        if ($xp->exists('KSK', $node) or $xp->exists('ZSK', $node)) {
            message_debug("Key %s active", $label);
            $arg_active = "-A now";
        } else {
            message_debug("Key %s not active", $label);
            $arg_active = "-A none";
        }

        # Build the command to extract the public key from the HSM
        my $cmd_extract = sprintf(
            "%s -K %s -E %s -l %s%s -a %d %s %s %s %s %s",
            $config->{'BIND'}->{'dnssec-keyfromlabel'},
            $dir,
            $config->{'BIND'}->{'engine'},
            $config->{'BIND'}->{'labelprefix'},
            $label,
            $algorithm,
            $arg_keyflag,
            $arg_publish,
            $arg_active,
            $arg_revoke,
            $zone
        );

        message_debug("> %s", $cmd_extract);

        my $keyfile;
        if ($fake) {
            $keyfile = sprintf("K_fake_%s", $label);
        } else {
            open(KEYFROMLABEL, "$cmd_extract |");
            $keyfile = <KEYFROMLABEL>;

            unless ($keyfile) {
                message_error("Failed extracting keys for zone %s, label %s",
                    $zone, $label);
                return ();
            }

            chomp $keyfile;
            close(KEYFROMLABEL);

            message_debug("Keyfile: %s", $keyfile);

            my $filename = sprintf("%s/%s", $dir, $keyfile);
            unless (-f "$filename.private") {
                message_error("Failed extracting keys for zone %s, label %s",
                    $zone, $label);
                return ();
            }
        }

        push @keyfiles, $keyfile;
    }

    # if any errors, do not return any key so this zone can be skipped
    if ($errors) {
        @keyfiles = ();
    }

    return @keyfiles;
}

sub sign_zone_offline {
    my $zone = shift;
    my $xp   = shift;

    my $zconfig = $config->{'OpenDNSSEC'}->{'zones'}->{$zone};

    # Set NSEC/NSEC3 options
    my $arg_denial = "";
    unless ($zconfig->{nsec}) {
        $arg_denial = sprintf("-H %d -3 %s",
            $zconfig->{nsec3}->{iterations},
            $zconfig->{nsec3}->{salt});

        if ($zconfig->{nsec3}->{optout}) {
            $arg_denial = sprintf("%s -A", $arg_denial);
        }
    }

    # Set serial number format
    my $arg_serialformat = undef;
    if ($zconfig->{serialformat} eq "unixtime") {
        $arg_serialformat = "unixtime";
    } elsif ($zconfig->{serialformat} eq "keep") {
        $arg_serialformat = "keep";
    } else {
        die;
    }

    # Build the command to sign the zone
    my $cmd_signzone = sprintf(
        "%s -t -S -x -K %s -N %s "
          . "-o %s -f %s -s %s -e %s -i %s -j %s -n %d -E %s " . "%s %s",
        $config->{'BIND'}->{'dnssec-signzone'},
        $config->{'BIND'}->{'keysdir'},
        $arg_serialformat,
        $zone,
        $zconfig->{output},
        $zconfig->{start},
        $zconfig->{end},
        $zconfig->{refresh},
        $zconfig->{jitter},
        $config->{'BIND'}->{'ncpu'},
        $config->{'BIND'}->{'engine'},
        $arg_denial,
        $zconfig->{input}
    );

    message_debug("> %s", $cmd_signzone);
    system($cmd_signzone) unless ($fake);
}

sub sign_zone_online {
    my $zone = shift;
    my $xp   = shift;

    my $cmd_rndc = sprintf("%s sign %s", $config->{'BIND'}->{'rndc'}, $zone);

    message_debug("> %s", $cmd_rndc);
    system($cmd_rndc) unless ($fake);
}

sub message_debug {
    return unless ($debug);
    printf("DEBUG: ");
    printf(@_);
    printf("\n");
}

sub message_error {
    printf("ERROR: ");
    printf(@_);
    printf("\n");
}

sub message_info {
    printf("INFO: ");
    printf(@_);
    printf("\n");
}

main();

__END__

=head1 NAME

ods4bind.pl - OpenDNSSEC for BIND interface

=head1 SYNOPSIS

ods4bind.pl [options]

Options:

 --help               brief help message
 --fake               do not actually execute any commands (just print)
 --config=FILENAME    set configuration filename
 --online             force online mode
 --offline            force offline mode
