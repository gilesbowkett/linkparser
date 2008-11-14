/*************************************************************************/
/* Copyright (c) 2004                                                    */
/* Daniel Sleator, David Temperley, and John Lafferty                    */
/* All rights reserved                                                   */
/*                                                                       */
/* Use of the link grammar parsing system is subject to the terms of the */
/* license set forth in the LICENSE file included with this software,    */
/* and also available at http://www.link.cs.cmu.edu/link/license.html    */
/* This license allows free redistribution and use in source and binary  */
/* forms, with or without modification, subject to certain conditions.   */
/*                                                                       */
/*************************************************************************/

/* stuff for transforming a dictionary entry into a disjunct list */

#include <link-grammar/api.h>

/*Temporary connectors used while converting expressions into disjunct lists */
typedef struct Tconnector_struct Tconnector;
struct Tconnector_struct{
	char multi;   /* TRUE if this is a multi-connector */
	char dir;	 /* '-' for left and '+' for right */
	Tconnector * next;
	char * string;
};

typedef struct clause_struct Clause;
struct clause_struct {
	Clause * next;
	int cost;
	int maxcost;
	Tconnector * c;
};

static Tconnector * copy_Tconnectors(Tconnector * c) {
/* This builds a new copy of the connector list pointed to by c.
   Strings, as usual, are not copied.
*/
	Tconnector *c1;
	if (c == NULL) return NULL;
	c1 = (Tconnector *) xalloc(sizeof(Tconnector));
	*c1 = *c;
	c1->next = copy_Tconnectors(c->next);
	return c1;
}

static void free_Tconnectors(Tconnector *e) {
	Tconnector * n;
	for(;e != NULL; e=n) {
		n = e->next;
		xfree((char *)e, sizeof(Tconnector));
	}
}

static void free_clause_list(Clause *c) {
	Clause *c1;
	while (c != NULL) {
		c1 = c->next;
		free_Tconnectors(c->c);
		xfree((char *)c, sizeof(Clause));
		c = c1;
	}
}

#ifdef UNUSED_FUNCTION
static Clause * copy_clause(Clause * d) {
/* This builds a new copy of the clause pointed to by d (except for the
   next field which is set to NULL).  Strings, as usual, are not copied.
*/
	Clause * d1;
	if (d == NULL) return NULL;
	d1 = (Clause *) xalloc(sizeof(Clause));
	*d1 = *d;
	d1->next = NULL;
	d1->c = copy_Tconnectors(d->c);
	return d1;
}
#endif /* UNUSED_FUNCTION */

static Tconnector * Treverse(Tconnector *e) {
/* reverse the order of the list e.  destructive */
	Tconnector * head, *x;
	head = NULL;
	while (e != NULL) {
		x = e->next;
		e->next = head;
		head = e;
		e = x;
	}
	return head;
}

static Connector * reverse(Connector *e) {
/* reverse the order of the list e.  destructive */
	Connector * head, *x;
	head = NULL;
	while (e != NULL) {
		x = e->next;
		e->next = head;
		head = e;
		e = x;
	}
	return head;
}

static Tconnector * catenate(Tconnector * e1, Tconnector * e2) {
/* Builds a new list of connectors that is the catenation of e1 with e2.
   does not effect lists e1 or e2.   Order is maintained. */

	Tconnector * e, * head;
	head = NULL;
	for (;e1 != NULL; e1 = e1->next) {
		e = (Tconnector *) xalloc(sizeof(Tconnector));
		*e = *e1;
		e->next = head;
		head = e;
	}
	for (;e2 != NULL; e2 = e2->next) {
		e = (Tconnector *) xalloc(sizeof(Tconnector));
		*e = *e2;
		e->next = head;
		head = e;
	}
	return Treverse(head);
}

static Tconnector * build_terminal(Exp * e) {
	/* build the connector for the terminal node n */
	Tconnector * c;
	c = (Tconnector *) xalloc(sizeof(Tconnector));
	c->string = e->u.string;
	c->multi = e->multi;
	c->dir = e->dir;
	c->next = NULL;
	return c;
}

static int maxcost_of_expression(Exp *e) {
	E_list * e_list;
	int m, m1;

	m = 0;

	if ((e->type == AND_type) || (e->type == OR_type)) {
		for (e_list = e->u.l; e_list != NULL; e_list = e_list->next) {
			m1 = maxcost_of_expression(e_list->e);
			m = MAX(m, m1);
		}
	}
	return (m + e->cost);
}

#ifdef UNUSED_FUNCTION
static int maxcost_of_sentence(Sentence sent) {
/* This returns the maximum maxcost of any disjunct in the sentence */
/* assumes the sentence expressions have been constructed */
	X_node * x;
	int w, m, m1;
	m = 0;

	for (w=0; w<sent->length; w++) {
		for (x=sent->word[w].x; x!=NULL; x = x->next){
			m1 = maxcost_of_expression(x->exp),
			m = MAX(m, m1);
		}
	}
	return m;
}
#endif /* UNUSED_FUNCTION */


static Clause * build_clause(Exp *e, int cost_cutoff) {
/* Build the clause for the expression e.  Does not change e */
	Clause *c=NULL, *c1, *c2, *c3, *c4, *c_head;
	E_list * e_list;

	assert(e != NULL, "build_clause called with null parameter");
	if (e->type == AND_type) {
		c1 = (Clause *) xalloc(sizeof (Clause));
		c1->c = NULL;
		c1->next = NULL;
		c1->cost = 0;
		c1->maxcost = 0 ;
		for (e_list = e->u.l; e_list != NULL; e_list = e_list->next) {
			c2 = build_clause(e_list->e, cost_cutoff);
			c_head = NULL;
			for (c3 = c1; c3 != NULL; c3 = c3->next) {
				for (c4 = c2; c4 != NULL; c4 = c4->next) {
					c = (Clause *) xalloc(sizeof (Clause));
					c->cost = c3->cost + c4->cost;
					c->maxcost = MAX(c3->maxcost,c4->maxcost);
					c->c = catenate(c3->c, c4->c);
					c->next = c_head;
					c_head = c;
				}
			}
			free_clause_list(c1);
			free_clause_list(c2);
			c1 = c_head;
		}
		c = c1;
	} else if (e->type == OR_type) {
		/* we'll catenate the lists of clauses */
		c = NULL;
		for (e_list = e->u.l; e_list != NULL; e_list = e_list->next) {
			c1 = build_clause(e_list->e, cost_cutoff);
			while(c1 != NULL) {
				c3 = c1->next;
				c1->next = c;
				c = c1;
				c1 = c3;
			}
		}
	} else if (e->type == CONNECTOR_type) {
		c = (Clause *) xalloc(sizeof(Clause));
		c->c = build_terminal(e);
		c->cost = 0;
		c->maxcost = 0;
		c->next = NULL;
	} else {
		assert(FALSE, "an expression node with no type");
	}

	/* c now points to the list of clauses */

	for (c1=c; c1!=NULL; c1 = c1->next) {
		c1->cost += e->cost;
/*		c1->maxcost = MAX(c1->maxcost,e->cost);  */  /* <---- This is how Dennis had it.
														I prefer the line below */
		c1->maxcost += e->cost;
	}
	return c;
}

#ifdef UNUSED_FUNCTION
static void print_connector_list(Connector * e) {
	for (;e != NULL; e=e->next) {
		printf("%s",e->string);
		if (e->label != NORMAL_LABEL) {
			printf("%3d", e->label);
		} else {
			printf("   ");
		}
		if (e->next != NULL) printf(" ");
	}
}

static void print_Tconnector_list(Tconnector * e) {
	for (;e != NULL; e=e->next) {
		if (e->multi) printf("@");
		printf("%s",e->string);
		printf("%c", e->dir);
		if (e->next != NULL) printf(" ");
	}
}

static void print_clause_list(Clause * c) {
	for (;c != NULL; c=c->next) {
		printf("  Clause: ");
		printf("(%2d, %2d)", c->cost, c->maxcost);
		print_Tconnector_list(c->c);
		printf("\n");
	}
}

static void print_disjunct_list(Disjunct * c) {
	for (;c != NULL; c=c->next) {
		printf("%10s: ", c->string);
		printf("(%2d)", c->cost);
		print_connector_list(c->left);
		printf(" <--> ");
		print_connector_list(c->right);
		printf("\n");
	}
}
#endif /* UNUSED_FUNCTION */

/**
 * Build a new list of connectors starting from the Tconnectors
 * in the list pointed to by e.  Keep only those whose strings whose
 * direction has the value c.
 */
static Connector * extract_connectors(Tconnector *e, int c)
{
	Connector *e1;
	if (e == NULL) return NULL;
	if (e->dir == c) {
		e1 = init_connector((Connector *) xalloc(sizeof(Connector)));
		e1->next = extract_connectors(e->next,c);
		e1->multi = e->multi;
		e1->string = e->string;
		e1->label = NORMAL_LABEL;
		e1->priority = THIN_priority;
		e1->word = 0;
		return e1;
	} else {
		return extract_connectors(e->next,c);
	}
}

/**
 * build a disjunct list out of the clause list c.
 * string is the print name of word that generated this disjunct.
 */
static Disjunct * 
build_disjunct(Clause * cl, const char * string, int cost_cutoff)
{
	Disjunct *dis, *ndis;
	dis = NULL;
	for (;cl != NULL; cl=cl->next) {
		if (cl->maxcost <= cost_cutoff) {
			ndis = (Disjunct *) xalloc(sizeof(Disjunct));
			ndis->left = reverse(extract_connectors(cl->c, '-'));
			ndis->right = reverse(extract_connectors(cl->c, '+'));
			ndis->string = string;
			ndis->cost = cl->cost;
			ndis->next = dis;
			dis = ndis;
		}
	}
	return dis;
}

static Disjunct * build_disjuncts_for_X_node(X_node * x, int cost_cutoff)
{
	Clause *c ;
	Disjunct * dis;
	c = build_clause(x->exp, cost_cutoff);
	dis = build_disjunct(c, x->string, cost_cutoff);
	free_clause_list(c);
	return dis;
}

Disjunct * build_disjuncts_for_dict_node(Dict_node *dn)
{
/* still need this for counting the number of disjuncts */
	Clause *c ;
	Disjunct * dis;
/*				 print_expression(dn->exp);   */
/*				 printf("\n");				*/
	c = build_clause(dn->exp, NOCUTOFF);
/*				 print_clause_list(c);		*/
	dis = build_disjunct(c, dn->string, NOCUTOFF);
	free_clause_list(c);
	return dis;
}

#if 0
OBS
OBS Disjunct * build_word_disjuncts(char * s) {
OBS /* Looks up the word s in the dictionary.  Returns NULL if it's not there.
OBS	If there, it builds the disjunct structure for the word, and returns
OBS	a pointer to it.
OBS */
OBS	 Dict_node * dn;
OBS	 Disjunct * dis;
OBS
OBS	 dn = dictionary_lookup(s);
OBS
OBS /*
OBS	 x = dn;
OBS	 printf("%s :", s);
OBS	 while (dn != NULL) {
OBS		printf("%s \n", dn->string);
OBS		print_expression(dn->node);
OBS		dn = dn->right;
OBS	 }
OBS	 dn = x;
OBS */
OBS	 dis = NULL;
OBS
OBS	 while (dn != NULL) {
OBS		 dis = catenate_disjuncts(build_disjuncts_for_dict_node(dn), dis);
OBS		 dn = dn->right;
OBS	 }
OBS				 /*	print_disjunct_list(dis); */
OBS	 return dis;
OBS }
#endif

/**
 * build_word_expressions() -- build list of expressions for a word
 *
 * Looks up the word s in the dictionary.  Returns NULL if it's not there.
 * If there, it builds the list of expressions for the word, and returns
 * a pointer to it.
 */
X_node * build_word_expressions(Sentence sent, const char * s)
{
	Dict_node * dn, *dn_head;
	X_node * x, * y;

	dn_head = dictionary_lookup_list(sent->dict, s);
	dn = dn_head;

	x = NULL;
	while (dn != NULL) {
		y = (X_node *) xalloc(sizeof(X_node));
		y->next = x;
		x = y;
		x->exp = copy_Exp(dn->exp);
		x->string = dn->string;
		dn = dn->right;
	}
	free_lookup_list (dn_head);
	return x;
}

/**
 * We've already built the sentence expressions.  This turns them into
 * disjuncts.
 */
void build_sentence_disjuncts(Sentence sent, int cost_cutoff)
{
	Disjunct * d;
	X_node * x;
	int w;
	for (w=0; w<sent->length; w++) {
		d = NULL;
		for (x=sent->word[w].x; x!=NULL; x = x->next){
			d = catenate_disjuncts(build_disjuncts_for_X_node(x, cost_cutoff),d);
		}
		sent->word[w].d = d;
	}
}
