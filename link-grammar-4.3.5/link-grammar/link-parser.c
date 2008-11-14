/***************************************************************************/
/* Copyright (c) 2004                                                      */
/* Daniel Sleator, David Temperley, and John Lafferty                      */
/* All rights reserved                                                     */
/*                                                                         */
/* Use of the link grammar parsing system is subject to the terms of the   */
/* license set forth in the LICENSE file included with this software,      */
/* and also available at http://www.link.cs.cmu.edu/link/license.html      */
/* This license allows free redistribution and use in source and binary    */
/* forms, with or without modification, subject to certain conditions.     */
/*                                                                         */
/***************************************************************************/

 /****************************************************************************
 *
 *   This is a simple example of the link parser API.  It similates most of
 *   the functionality of the original link grammar parser, allowing sentences
 *   to be typed in either interactively or in "batch" mode (if -batch is
 *   specified on the command line, and stdin is redirected to a file).
 *   The program:
 *     Opens up a dictionary
 *     Iterates:
 *        1. Reads from stdin to get an input string to parse
 *        2. Tokenizes the string to form a Sentence
 *        3. Tries to parse it with cost 0
 *        4. Tries to parse with increasing cost
 *     When a parse is found:
 *        1. Extracts each Linkage
 *        2. Passes it to process_some_linkages()
 *        3. Deletes linkage
 *     After parsing each Sentence is deleted by making a call to
 *     sentence_delete.
 *
 ****************************************************************************/

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef HAVE_EDITLINE
#include <editline/readline.h>
#endif 

#include <link-grammar/link-includes.h>
#include <link-grammar/structures.h>
#include <link-grammar/error.h>

#include "command-line.h"

#define MAX_INPUT 1024
#define DISPLAY_MAX 1024
#define COMMENT_CHAR '%'  /* input lines beginning with this are ignored */

static int batch_errors = 0;
static int input_pending=FALSE;
static Parse_Options  opts;
static Parse_Options  panic_parse_opts;
static int verbosity = 0;

typedef enum
{
	UNGRAMMATICAL='*',
	PARSE_WITH_DISJUNCT_COST_GT_0=':',
	NO_LABEL=' '
} Label;

static char *
fget_input_string(FILE *in, FILE *out, Parse_Options opts)
{
#ifdef HAVE_EDITLINE
	static char * pline = NULL;
	const char * prompt = "linkparser> ";

	if (input_pending && pline != NULL)
	{
		input_pending = FALSE;
		return pline;
	}
	if (parse_options_get_batch_mode(opts) ||
	    (verbosity == 0) ||
	    input_pending)
	{
		prompt = "";
	}
	input_pending = FALSE;
	if (pline) free(pline);
	pline = readline(prompt);

	/* Save non-blank lines */
	if (pline && *pline)
	{
		if (*pline) add_history(pline);
	}
	return pline;

#else
	static char input_string[MAX_INPUT];

	if ((!parse_options_get_batch_mode(opts)) && 
	    (verbosity > 0) && 
	    (!input_pending))
	{
		fprintf(out, "linkparser> ");
		fflush(out);
	}
	input_pending = FALSE;

	/* For UTF-8 input, I think its still technically correct to
	 * use fgets() and not fgetws() at this point. */
	if (fgets(input_string, MAX_INPUT, in)) return input_string;
	else return NULL;
#endif
}

static int fget_input_char(FILE *in, FILE *out, Parse_Options opts)
{
#ifdef HAVE_EDITLINE
	char * pline = fget_input_string(in, out, opts);
	if (NULL == pline) return EOF;
	if (*pline)
	{
		input_pending = TRUE;
		return *pline;
	}
	return '\n';

#else
	int c;

	if (!parse_options_get_batch_mode(opts) && (verbosity > 0))
		fprintf(out, "linkparser> ");
	fflush(out);

	/* For UTF-8 input, I think its still technically correct to
	 * use fgetc() and not fgetwc() at this point. */
	c = fgetc(in);
	if (c != '\n')
	{
		ungetc(c, in);
		input_pending = TRUE;
	}
	return c;
#endif
}

/**************************************************************************
*
*  This procedure displays a linkage graphically.  Since the diagrams
*  are passed as character strings, they need to be deleted with a
*  call to free.
*
**************************************************************************/

static void process_linkage(Linkage linkage, Parse_Options opts)
{
	char * string;
	int j, mode, first_sublinkage;
	int nlink;

	if (!linkage) return;  /* Can happen in timeout mode */

	if (parse_options_get_display_union(opts)) {
		linkage_compute_union(linkage);
		first_sublinkage = linkage_get_num_sublinkages(linkage)-1;
	}
	else {
		first_sublinkage = 0;
	}

	nlink = linkage_get_num_sublinkages(linkage);
	for (j=first_sublinkage; j<nlink; ++j)
	{
		linkage_set_current_sublinkage(linkage, j);
		if (parse_options_get_display_on(opts)) {
			string = linkage_print_diagram(linkage);
			fprintf(stdout, "%s", string);
			free(string);
		}
		if (parse_options_get_display_links(opts)) {
			string = linkage_print_links_and_domains(linkage);
			fprintf(stdout, "%s", string);
			free(string);
		}
		if (parse_options_get_display_postscript(opts)) {
			string = linkage_print_postscript(linkage, FALSE);
			fprintf(stdout, "%s\n", string);
			free(string);
		}
	}
	if ((mode = parse_options_get_display_constituents(opts)))
	{
		string = linkage_print_constituent_tree(linkage, mode);
		if (string != NULL) {
			fprintf(stdout, "%s\n", string);
			free(string);
		} else {
			fprintf(stderr, "Can't generate constituents.\n");
			fprintf(stderr, "Constituent processing has been turned off.\n");
		}
	}
}

static void print_parse_statistics(Sentence sent, Parse_Options opts)
{
	if (sentence_num_linkages_found(sent) > 0) {
		if (sentence_num_linkages_found(sent) >
			parse_options_get_linkage_limit(opts)) {
			fprintf(stdout, "Found %d linkage%s (%d of %d random " \
					"linkages had no P.P. violations)",
					sentence_num_linkages_found(sent),
					sentence_num_linkages_found(sent) == 1 ? "" : "s",
					sentence_num_valid_linkages(sent),
					sentence_num_linkages_post_processed(sent));
		}
		else {
			fprintf(stdout, "Found %d linkage%s (%d had no P.P. violations)",
					sentence_num_linkages_post_processed(sent),
					sentence_num_linkages_found(sent) == 1 ? "" : "s",
					sentence_num_valid_linkages(sent));
		}
		if (sentence_null_count(sent) > 0) {
			fprintf(stdout, " at null count %d", sentence_null_count(sent));
		}
		fprintf(stdout, "\n");
	}
}


static int process_some_linkages(Sentence sent, Parse_Options opts)
{
	int c;
	int i, num_displayed, num_to_query;
	Linkage linkage;

	if (verbosity > 0) print_parse_statistics(sent, opts);
	if (!parse_options_get_display_bad(opts)) {
		num_to_query = MIN(sentence_num_valid_linkages(sent), DISPLAY_MAX);
	}
	else {
		num_to_query = MIN(sentence_num_linkages_post_processed(sent),
		                   DISPLAY_MAX);
	}

	for (i=0, num_displayed=0; i<num_to_query; i++)
	{
		if ((sentence_num_violations(sent, i) > 0) &&
			(!parse_options_get_display_bad(opts))) {
			continue;
		}

		linkage = linkage_create(i, sent, opts);

		if (verbosity > 0) {
			if ((sentence_num_valid_linkages(sent) == 1) &&
				(!parse_options_get_display_bad(opts)))
			{
				fprintf(stdout, "	Unique linkage, ");
			}
			else if ((parse_options_get_display_bad(opts)) &&
			         (sentence_num_violations(sent, i) > 0))
			{
				fprintf(stdout, "	Linkage %d (bad), ", i+1);
			}
			else
			{
				fprintf(stdout, "	Linkage %d, ", i+1);
			}

			if (!linkage_is_canonical(linkage)) {
				fprintf(stdout, "non-canonical, ");
			}
			if (linkage_is_improper(linkage)) {
				fprintf(stdout, "improper fat linkage, ");
			}
			if (linkage_has_inconsistent_domains(linkage)) {
				fprintf(stdout, "inconsistent domains, ");
			}

			fprintf(stdout, "cost vector = (UNUSED=%d DIS=%d AND=%d LEN=%d)\n",
			       linkage_unused_word_cost(linkage),
			       linkage_disjunct_cost(linkage),
			       linkage_and_cost(linkage),
			       linkage_link_cost(linkage));
		}

		process_linkage(linkage, opts);
		linkage_delete(linkage);

		if (++num_displayed < num_to_query) {
			if (verbosity > 0) {
				fprintf(stdout, "Press RETURN for the next linkage.\n");
			}
			c = fget_input_char(stdin, stdout, opts);
			if (c != '\n') return c;
		}
	}
	return 'x';
}

static int there_was_an_error(Label label, Sentence sent, Parse_Options opts)
{
	if (sentence_num_valid_linkages(sent) > 0) {
		if (label == UNGRAMMATICAL) {
			batch_errors++;
			return UNGRAMMATICAL;
		}
		if ((sentence_disjunct_cost(sent, 0) == 0) &&
			(label == PARSE_WITH_DISJUNCT_COST_GT_0)) {
			batch_errors++;
			return PARSE_WITH_DISJUNCT_COST_GT_0;
		}
	} else {
		if (label != UNGRAMMATICAL) {
			batch_errors++;
			return UNGRAMMATICAL;
		}
	}
	return FALSE;
}

static void batch_process_some_linkages(Label label, 
                                        Sentence sent,
                                        Parse_Options opts)
{
	Linkage linkage;

	if (there_was_an_error(label, sent, opts)) {
		if (sentence_num_linkages_found(sent) > 0) {
			linkage = linkage_create(0, sent, opts);
			process_linkage(linkage, opts);
			linkage_delete(linkage);
		}
		fprintf(stdout, "+++++ error %d\n", batch_errors);
	}
}

static int special_command(char *input_string, Dictionary dict)
{
	if (input_string[0] == '\n') return TRUE;
	if (input_string[0] == COMMENT_CHAR) return TRUE;
	if (input_string[0] == '!') {
		if (strncmp(input_string, "!panic_", 7)==0) {
			issue_special_command(input_string+7, panic_parse_opts, dict);
		}
		else {
			issue_special_command(input_string+1, opts, dict);
		}
		return TRUE;
	}
	return FALSE;
}

static Label strip_off_label(char * input_string)
{
	int c;

	c = input_string[0];
	switch(c) {
	case UNGRAMMATICAL:
	case PARSE_WITH_DISJUNCT_COST_GT_0:
		input_string[0] = ' ';
		return c;
		break;
	default:
		return NO_LABEL;
	}
}

static void setup_panic_parse_options(Parse_Options opts)
{
	parse_options_set_disjunct_cost(opts, 3);
	parse_options_set_min_null_count(opts, 1);
	parse_options_set_max_null_count(opts, MAX_SENTENCE);
	parse_options_set_max_parse_time(opts, 60);
	parse_options_set_islands_ok(opts, 1);
	parse_options_set_short_length(opts, 6);
	parse_options_set_all_short_connectors(opts, 1);
	parse_options_set_linkage_limit(opts, 100);
}

static void print_usage(char *str) {
	fprintf(stderr,
			"Usage: %s [language]\n"
			"		  [-ppoff] [-coff] [-aoff] [-batch] [-<special \"!\" command>]\n", str);
	exit(-1);
}

int main(int argc, char * argv[])
{
	Dictionary      dict;
	Sentence        sent;
	const char     *language="en";  /* default to english, and not locale */
	int             pp_on=TRUE;
	int             af_on=TRUE;
	int             cons_on=TRUE;
	int             num_linkages, i;
	char            *input_string;
	Label           label = NO_LABEL;

	i = 1;
	if ((argc > 1) && (argv[1][0] != '-')) {
		/* the dictionary is the first argument if it doesn't begin with "-" */
		language = argv[1];
		i++;
	}

	/* Get the locale from the environment... 
	 * perhaps we should someday get it from the dictionary ??
	 */
	setlocale(LC_ALL, "");

	for (; i<argc; i++) {
		if (argv[i][0] == '-') {
			if (strcmp("-ppoff", argv[i])==0) {
				pp_on = FALSE;
			} else if (strcmp("-coff", argv[i])==0) {
				cons_on = FALSE;
			} else if (strcmp("-aoff", argv[i])==0) {
				af_on = FALSE;
			} else if (strcmp("-batch", argv[i])==0) {
			} else if (strncmp("-!", argv[i],2)==0) {
			} else {
				print_usage(argv[0]);
			}
		} else {
			print_usage(argv[0]);
		}
	}

	opts = parse_options_create();
	if (opts == NULL) {
		fprintf(stderr, "%s\n", lperrmsg);
		exit(-1);
	}

	panic_parse_opts = parse_options_create();
	if (panic_parse_opts == NULL) {
		fprintf(stderr, "%s\n", lperrmsg);
		exit(-1);
	}
	setup_panic_parse_options(panic_parse_opts);
	parse_options_set_max_sentence_length(opts, 70);
	parse_options_set_panic_mode(opts, TRUE);
	parse_options_set_max_parse_time(opts, 30);
	parse_options_set_linkage_limit(opts, 1000);
	parse_options_set_short_length(opts, 10);

	if(language && *language)
		dict = dictionary_create_lang(language);
	else
		dict = dictionary_create_default_lang();

	if (dict == NULL) {
		fprintf(stderr, "%s\n", lperrmsg);
		exit(-1);
	}

	/* process the command line like commands */
	for (i=1; i<argc; i++) {
		if ((strcmp("-pp", argv[i])==0) ||
			(strcmp("-c", argv[i])==0) ||
			(strcmp("-a", argv[i])==0))
		{
			i++;
		}
		else if ((argv[i][0] == '-') && (strcmp("-ppoff", argv[i])!=0) &&
		         (argv[i][0] == '-') && (strcmp("-coff", argv[i])!=0) &&
		         (argv[i][0] == '-') && (strcmp("-aoff", argv[i])!=0))
		{
			issue_special_command(argv[i]+1, opts, dict);
		}
	}

	verbosity = parse_options_get_verbosity(opts);

	/* Main input loop */
	while (NULL != (input_string = fget_input_string(stdin, stdout, opts)))
	{
		if ((strcmp(input_string, "quit\n")==0) ||
			(strcmp(input_string, "exit\n")==0)) break;

		if (special_command(input_string, dict)) continue;
		if (parse_options_get_echo_on(opts)) {
			printf("%s", input_string);
		}

		if (parse_options_get_batch_mode(opts)) {
			label = strip_off_label(input_string);
		}

		sent = sentence_create(input_string, dict);

		if (sent == NULL) {
			if (verbosity > 0) fprintf(stderr, "%s\n", lperrmsg);
			/* if (lperrno != NOTINDICT) exit(-1); */
			continue;
		}
		if (sentence_length(sent) > parse_options_get_max_sentence_length(opts)) {
			if (verbosity > 0) {
				fprintf(stdout,
				       "Sentence length (%d words) exceeds maximum allowable (%d words)\n",
					sentence_length(sent), parse_options_get_max_sentence_length(opts));
			}
			sentence_delete(sent);
			continue;
		}

		/* First parse with cost 0 or 1 and no null links */
		parse_options_set_disjunct_cost(opts, 2);
		parse_options_set_min_null_count(opts, 0);
		parse_options_set_max_null_count(opts, 0);
		parse_options_reset_resources(opts);

		num_linkages = sentence_parse(sent, opts);

		/* Now parse with null links */
		if ((num_linkages == 0) && (!parse_options_get_batch_mode(opts))) {
			if (verbosity > 0) fprintf(stdout, "No complete linkages found.\n");
			if (parse_options_get_allow_null(opts)) {
				parse_options_set_min_null_count(opts, 1);
				parse_options_set_max_null_count(opts, sentence_length(sent));
				num_linkages = sentence_parse(sent, opts);
			}
		}

		if (parse_options_timer_expired(opts)) {
			if (verbosity > 0) fprintf(stdout, "Timer is expired!\n");
		}
		if (parse_options_memory_exhausted(opts)) {
			if (verbosity > 0) fprintf(stdout, "Memory is exhausted!\n");
		}

		if ((num_linkages == 0) &&
			parse_options_resources_exhausted(opts) &&
			parse_options_get_panic_mode(opts)) {
			/* print_total_time(opts); */
			if (verbosity > 0) fprintf(stdout, "Entering \"panic\" mode...\n");
			parse_options_reset_resources(panic_parse_opts);
			parse_options_set_verbosity(panic_parse_opts, verbosity);
			num_linkages = sentence_parse(sent, panic_parse_opts);
			if (parse_options_timer_expired(panic_parse_opts)) {
				if (verbosity > 0) fprintf(stdout, "Timer is expired!\n");
			}
		}

		/* print_total_time(opts); */

		if (parse_options_get_batch_mode(opts)) {
			batch_process_some_linkages(label, sent, opts);
		}
		else {
			int c = process_some_linkages(sent, opts);
			if (c == EOF) break;
		}

		sentence_delete(sent);
	}

	if (parse_options_get_batch_mode(opts)) {
		/* print_time(opts, "Total"); */
		fprintf(stderr,
				"%d error%s.\n", batch_errors, (batch_errors==1) ? "" : "s");
	}

	parse_options_delete(panic_parse_opts);
	parse_options_delete(opts);
	dictionary_delete(dict);

	printf ("Bye.\n");
	return 0;
}
