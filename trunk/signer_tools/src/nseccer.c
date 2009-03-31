/**
 * This tool creates NSEC records
 *
 * This code is provided AS-IS, you know the drill, use at own risk
 * 
 * Input must be sorted
 * 
 * Written by Jelte Jansen
 * 
 * Copyright 2008 NLnet Labs
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <ldns/ldns.h>
#include "util.h"

bool
is_same_name(ldns_rr *a, ldns_rr *b)
{
	if (!a || !b) {
		return false;
	} else if (ldns_dname_compare(ldns_rr_owner(a),
	                              ldns_rr_owner(b)) != 0) {
		return false;
	} else {
		return true;
	}
}

void
usage(FILE *out)
{
	fprintf(out, "Usage: nseccer [options]\n");
	fprintf(out, "Options:\n");
	fprintf(out, "-f <file>\tRead RR's from file instead of stdin\n");
	fprintf(out, "-h\t\tShow this text\n");
	fprintf(out, "-n\t\tDon't echo the input records\n");
	fprintf(out, "-v <level>\tVerbosity level\n");
	fprintf(out, "\n");
	fprintf(out, "When a new owner name is read (or input stops),\n");
	fprintf(out, "an NSEC record is created from the previous to\n");
	fprintf(out, "the new owner name. All rr types seen with the\n");
	fprintf(out, "previous owner name are added to this new NSEC\n");
	fprintf(out, "Resource Record\n");
	fprintf(out, "These records are then printed to stdout\n");
}

void
make_nsec(ldns_rr *to, uint32_t ttl, ldns_rr_list *rr_list, ldns_rr **first_nsec)
{
	ldns_rr *nsec_rr;
	
	/* handle rrset */
	if (1) {
		ldns_rr_list_print(stdout, rr_list);
	}
	
	/* create nsec and print it */
	nsec_rr = ldns_create_nsec(ldns_rr_list_owner(rr_list),
							   ldns_rr_owner(to),
							   rr_list);
	ldns_rr_set_ttl(nsec_rr, ttl);
	ldns_rr_print(stdout, nsec_rr);

	if (first_nsec && !(*first_nsec)) {
		*first_nsec = ldns_rr_clone(nsec_rr);
	}

	/* clean for next set */
	rr_list_clear(rr_list);
	ldns_rr_free(nsec_rr);
	ldns_rr_list_push_rr(rr_list, to);
}

void
handle_name(ldns_rr *rr, uint32_t soa_min_ttl, ldns_rr_list *rr_list, ldns_rr **prev_nsec, ldns_rr **first_nsec)
{
	if (rr && ldns_rr_list_rr_count(rr_list) > 0) {
		if (ldns_dname_compare(ldns_rr_owner(rr), ldns_rr_list_owner(rr_list)) == 0) {
			ldns_rr_list_push_rr(rr_list, rr);
		} else {
			make_nsec(rr, soa_min_ttl, rr_list, first_nsec);
		}
	} else if (rr) {
		ldns_rr_list_push_rr(rr_list, rr);
	}
}

ldns_status
handle_line(const char *line,
            int line_len,
            uint32_t soa_min_ttl,
            ldns_rr_list *rr_list,
            ldns_rr **prev_nsec,
            ldns_rr **first_nsec)
{
	ldns_rr *rr;
	ldns_status status;

	if (line_len > 0) {
		if (line[0] != ';') {
			status = ldns_rr_new_frm_str(&rr, line, 0, NULL, NULL);
			if (status == LDNS_STATUS_OK) {
				handle_name(rr, soa_min_ttl, rr_list, prev_nsec, first_nsec);
			} else {
				fprintf(stderr, "Error parsing RR (%s):\n; %s\n",
						ldns_get_errorstr_by_id(status), line);
				return status;
			}
		} else {
			/* comment line. pass */
			printf("%s\n", line);
		}
	}
	return LDNS_STATUS_OK;
}



ldns_status
create_nsec_records(FILE *input_file, uint32_t soa_min_ttl)
{
	char line[MAX_LINE_LEN];
	int line_len = 0;
	ldns_status result = LDNS_STATUS_OK;
	ldns_rr *rr;
	ldns_rr_list *rr_list;
	ldns_rr *prev_nsec;
	ldns_rr *first_nsec = NULL;
	
	char *pre_soa_lines[MAX_LINE_LEN];
	size_t pre_count = 0, i;
	if (soa_min_ttl == 0) {
		line_len = 0;
		while (line_len >= 0 && soa_min_ttl == 0) {
			line_len = read_line(input_file, line);
			pre_soa_lines[pre_count] = strdup(line);
			pre_count++;
			if (line_len > 0 && line[0] != ';') {
				result = ldns_rr_new_frm_str(&rr, line, 0, NULL, NULL);
				if (result == LDNS_STATUS_OK &&
					ldns_rr_get_type(rr) == LDNS_RR_TYPE_SOA) {
					soa_min_ttl = ldns_rdf2native_int32(ldns_rr_rdf(rr, 6));
					if (ldns_rr_ttl(rr) < soa_min_ttl) {
						soa_min_ttl = ldns_rr_ttl(rr);
					}
				}
				ldns_rr_free(rr);
			}
		}
	}

	rr_list = ldns_rr_list_new();
	/* ok, now handle the lines we skipped over */
	for (i = 0; i < pre_count; i++) {
		handle_line(pre_soa_lines[i], strlen(pre_soa_lines[i]),
		            soa_min_ttl, rr_list, &prev_nsec, &first_nsec);
		free(pre_soa_lines[i]);
	}

	/* and do the rest of the file */
	while (line_len >= 0) {
		line_len = read_line(input_file, line);
		if (line_len > 0) {
			handle_line(line, line_len, soa_min_ttl, rr_list, &prev_nsec, &first_nsec);
		}
	}

	/* and loop to start */
	if (ldns_rr_list_rr_count(rr_list) > 0 && first_nsec) {
		make_nsec(first_nsec, soa_min_ttl, rr_list, &first_nsec);
	}
	ldns_rr_list_deep_free(rr_list);

	return result;
}

int
main(int argc, char **argv)
{
	int verbosity = 5;
	int c;
	bool echo_input = true;
	FILE *input_file = stdin;

	ldns_status status;
	uint32_t soa_min_ttl = 0;

	while ((c = getopt(argc, argv, "f:nv:")) != -1) {
		switch(c) {
			case 'f':
				input_file = fopen(optarg, "r");
				if (!input_file) {
					fprintf(stderr,
					        "Error opening %s: %s\n",
					        optarg,
					        strerror(errno));
				}
				break;
			case 'h':
				usage(stderr);
				exit(0);
				break;
			case 'n':
				echo_input = false;
				break;
			case 'v':
				verbosity = atoi(optarg);
				break;
			default:
				usage(stderr);
				exit(1);
				break;
		}
	}
	

	status = create_nsec_records(input_file,
	                             soa_min_ttl);

	if (input_file != stdin) {
		fclose(input_file);
	}
	
	return 0;
}
