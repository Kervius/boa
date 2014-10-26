
/* Covered by GPLv2 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>


/* =======================================================

    Declarations

   ======================================================= */

#define DEF_QCH  "@"
#define DEF_OBR_ "{"
#define DEF_CBR_ "}"

#define DEF_OBR  (DEF_OBR_ [0])
#define DEF_CBR  (DEF_CBR_ [0])

#define DEF_OBR2  DEF_OBR_ DEF_OBR_
#define DEF_CBR2  DEF_CBR_ DEF_CBR_

#define BOA_VERSION "0.1"
#define BOA_USAGE "usage: boa [-v] [-h] [-l lang] [-Q " DEF_QCH "] [-d] [-o output-file] <input-file>\n"

typedef char *(*quote_func_t)(char *, size_t, size_t *);
struct lang_def {
	char *name;
	char *intrp[16];
	char *print_nl;
	quote_func_t quote;
	char *print_s;
	char *print_e;
	char *preamble;
	char *closure;
};

char *shell_quote(char *str, size_t ilen, size_t *len);
char *perl_quote(char *str, size_t ilen, size_t *len);
char *lua_quote(char *str, size_t ilen, size_t *len);
char *c_quote(char *str, size_t ilen, size_t *len);
struct lang_def lang_conf[] = {
	{
		.name = "shell",
		.intrp = { "/bin/sh", "/bin/sh", },
		.print_nl = "echo\n",
		.quote = shell_quote,
		.print_s = "echo -n ",
		.print_e = "\n",
		.preamble = "",
		.closure = "",
	},
	{
		.name = "bash",
		.intrp = { "/bin/bash", "/bin/bash", },
		.print_nl = "echo\n",
		.quote = shell_quote,
		.print_s = "echo -n ",
		.print_e = "\n",
		.preamble = "",
		.closure = "",
	},
	{
		.name = "dummy",
		.intrp = { "/bin/cat", "/bin/cat", },
		.print_nl = "echo\n",
		.quote = shell_quote,
		.print_s = "echo -n ",
		.print_e = "\n",
		.preamble = "# curtains open\n",
		.closure = "# curtains close\n" "# gooood niiiiight!\n",
	},
	{
		.name = "perl",
		.intrp = { "/usr/bin/env", "perl", "perl" },
		.print_nl = "print qq{\\n};\n",
		.quote = perl_quote,
		.print_s = "print ",
		.print_e = ";\n",
		.preamble = "",
		.closure = "",
	},
	{
		.name = "ruby",
		.intrp = { "/usr/bin/env", "ruby", "ruby" },
		.print_nl = "print \"\\n\";\n",
		.quote = perl_quote,
		.print_s = "print ",
		.print_e = ";\n",
		.preamble = "",
		.closure = "",
	},
	{
		.name = "lua",
		.intrp = { "/usr/bin/env", "lua", "lua" },
		.print_nl = "io.output():write('\\n')\n",
		.quote = lua_quote,
		.print_s = "io.output():write(",
		.print_e = ")\n",
		.preamble = "",
		.closure = "",
	},
#if 1 && 0  /* just for fun; not tested */
	{
		.name = "c",
		.intrp = { "/usr/bin/env", "tcc", "tcc", "-run", "-", },
		.print_nl = "putchar('\\n');\n",
		.quote = c_quote,
		.print_s = "printf( \"%s\", ",
		.print_e = ");\n",
		.preamble = "#include <stdio.h>\n" "int main() {\n",
		.closure = "return 0;\n" "}\n" ,
	},
#endif
};
#define LANG_COUNT (sizeof(lang_conf)/sizeof(struct lang_def))

/* =======================================================

    Functions

   ======================================================= */


int str_begins_with( char *str, const char *pref )
{
	return memcmp( str, pref, strlen(pref) ) == 0;
}

int str_begins_with2( char *str, const char *pref1, const char *pref2 )
{
	size_t len1 = strlen( pref1 );
	return (memcmp( str, pref1, len1 ) == 0) &&
		(memcmp( str + len1, pref2, strlen(pref2) ) == 0);
}

int str_begins_with_ss( char *str, const char *pref, char **p )
{
	size_t len = strlen(pref);
	while (isspace(*str)) str++;
	if (memcmp( str, pref, len ) == 0) {
		if (p) *p = str + len;
		return 1;
	}
	return 0;
}

int str_begins_with2_ss( char *str, const char *pref1, const char *pref2, char **p )
{
	size_t len1, len2;
	len1 = strlen( pref1 );
	len2 = strlen( pref2 );
	while (isspace(*str)) str++;
	if ( (memcmp( str, pref1, len1 ) == 0) &&
		(memcmp( str + len1, pref2, len2 ) == 0) ) 
	{
		if (p) *p = str + len1 + len2;
		return 1;
	}
	return 0;
}

int magic( char *str, size_t len, char **lang, char **qch )
{
	char ch;
	int l = 0;
	/* magic example: %%% bozo %%%
	 * qch -> "%"
	 * lang -> "bozo" */
	if (len > 8) {
		ch = str[0];
		if (!isspace(ch) && ch == str[1] && ch == str[2] && isspace(str[3])) {
			if (ch == str[len-1] && ch == str[len-2] && ch == str[len-3] && isspace(str[len-4])) {
				*qch = strndup(str, 1);
				str+=3;
				while (isspace(*str)) str++;
				while (!isspace(str[l])) { l++; }
				*lang = strndup(str, l);
				return 1;
			}
		}
	}
	return 0;
}

size_t chomp( char *str, size_t len )
{
	size_t l = len;
	while (l > 0)
		if (str[l-1] == '\n' || str[l-1] == '\r')
			str[--l] = 0;
		else
			break;
	return l;
}

char *fcb( char *str, char op, char cl )
{
	int count = 0;
	while (*str) {
		if (*str == '\\') {
			str++; str++; continue;
		}
		else if (*str == op) {
			count++;
		}
		else if (*str == cl) {
			count--;
			if (count == 0)
				return str;
			if (count < 0)
				return NULL;
		}
		str++;
	}
	return NULL;
}

static void check_capacity( char **buf, size_t *cap, size_t offs, char **curp, size_t curi )
{
	if (*cap < offs) {
		*cap = offs < 128 ? 256 : (*cap)*2;
		*buf = realloc( *buf, *cap );
		if (curp)
			*curp = *buf + curi;
	}
}

/* shell style string quoting: in single quotes, only single quote itself need escaping */
char *shell_quote(char *str, size_t ilen, size_t *len)
{
	static char *buf = NULL;
	static size_t buf_cap = 0;
	const char Q = '\'';
	const char E = '\\';

	check_capacity( &buf, &buf_cap, ilen, 0, 0 );
	if (len) *len = 0;

	if (buf) {
		size_t i = 0;
		char *p = buf;
		p[i++] = Q;
		while (ilen > 0) {
			check_capacity( &buf, &buf_cap, i+8, &p, i );
			if (!buf) return buf;
			if (*str != Q) {
				p[i++] = *str;
			}
			else {
				p[i++] = Q;
				p[i++] = E;
				p[i++] = Q;
				p[i++] = Q;
			}
			str++;
			ilen--;
		}
		p[i++] = Q;
		p[i] = 0;
		if (len) *len = i;
	}
	return buf;
}

/* perl style string quoting: in single quotes, single quote and back slash need escaping */
char *perl_quote(char *str, size_t ilen, size_t *len)
{
	static char *buf = NULL;
	static size_t buf_cap = 0;
	const char Q = '\'';
	const char E = '\\';

	check_capacity( &buf, &buf_cap, ilen, 0, 0 );
	if (len) *len = 0;

	if (buf) {
		size_t i = 0;
		char *p = buf;
		p[i++] = Q;
		while (ilen > 0) {
			check_capacity( &buf, &buf_cap, i+8, &p, i );
			if (!buf) return buf;
			if (*str == Q) {
				p[i++] = E;
				p[i++] = Q;
			}
			else if (*str == E) {
				p[i++] = E;
				p[i++] = E;
			}
			else {
				p[i++] = *str;
			}
			str++;
			ilen--;
		}
		p[i++] = Q;
		p[i] = 0;
		if (len) *len = i;
	}
	return buf;
}

/* c style string quoting: in double quotes, double quote and back slash need escaping */
char *c_quote(char *str, size_t ilen, size_t *len)
{
	static char *buf = NULL;
	static size_t buf_cap = 0;
	const char DQ = '"';
	const char E = '\\';

	check_capacity( &buf, &buf_cap, ilen, 0, 0 );
	if (len) *len = 0;

	if (buf) {
		size_t i = 0;
		char *p = buf;
		p[i++] = DQ;
		while (ilen > 0) {
			check_capacity( &buf, &buf_cap, i+8, &p, i );
			if (!buf) return buf;
			if (*str == DQ) {
				p[i++] = E;
				p[i++] = DQ;
			}
			else if (*str == E) {
				p[i++] = E;
				p[i++] = E;
			}
			else {
				p[i++] = *str;
			}
			str++;
			ilen--;
		}
		p[i++] = DQ;
		p[i] = 0;
		if (len) *len = i;
	}
	return buf;
}

/* lua style string quoting: in single quotes, single quote and back slash need escaping */
char *lua_quote(char *str, size_t ilen, size_t *len)
{
	static char *buf = NULL;
	static size_t buf_cap = 0;
	const char Q = '\'';
	const char E = '\\';

	check_capacity( &buf, &buf_cap, ilen, 0, 0 );
	if (len) *len = 0;

	if (buf) {
		size_t i = 0;
		char *p = buf;
		p[i++] = Q;
		while (ilen > 0) {
			check_capacity( &buf, &buf_cap, i+8, &p, 0 );
			if (!buf) return buf;
			if (*str == Q) {
				p[i++] = E;
				p[i++] = Q;
			}
			else if (*str == E) {
				p[i++] = E;
				p[i++] = E;
			}
			else {
				p[i++] = *str;
			}
			str++;
			ilen--;
		}
		p[i++] = Q;
		p[i] = 0;
		if (len) *len = i;
	}
	return buf;
}

static int streq( const char *str1, const char *str2 )
{
	return strcmp(str1,str2) == 0;
}

void cloexec( FILE *f )
{
	int fd = fileno(f);
	int fl, rc;
	fl = fcntl( fd, F_GETFD );
	rc = fcntl( fd, F_SETFD, fl|FD_CLOEXEC );
	(void)rc;
	fl = fcntl( fd, F_GETFD );
	if (!(fl & FD_CLOEXEC))
		fprintf( stderr, "cloexec failed!\n" );
}

FILE *prepare_output( char **prog, const char *ofn, int *child_pid )
{
	FILE *output = NULL;
	int pip[2];
	int pid, rc, rc2;
	const int RD = 0, WR = 1;

	if (ofn && !streq(ofn,"-")) {
		output = fopen( ofn, "w" );
		if (!output) {
			fprintf( stderr, "can't open output file: %s\n", ofn );
			return NULL;
		}
	}

	/* rc = socketpair(AF_UNIX,SOCK_STREAM,0,pip)? - sockets have deeper buffers */
	rc = pipe(pip);
	if (rc == -1) {
		fprintf( stderr, "can't create pipe!\n" );
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		fprintf( stderr, "can't fork. bring a spoon.\n" );
		return NULL;
	}

	if (pid) {
		/* parent */
		close( pip[RD] );
		if (output) fclose( output );
		if (child_pid) *child_pid = pid;
		return fdopen( pip[WR], "w" );
	}
	else {
		/* child */
		close( pip[WR] );
		/* stdin <- pip[RD] */
		/* stdout <- fileno(output) */
		rc = dup2( pip[RD], 0 );
		rc2 = output ? dup2( fileno(output), 1 ) : 0 ; /* pass-through out stdout */
		if (rc == -1 || rc2 == -1) {
			fprintf( stderr, "failed to setup I/O for the interpreter.\n" );
			exit(1);
		}
		close(pip[RD]);
		if (output) fclose(output);
		/*system( "ls -l /proc/self/fd/" );*/
		rc = execv( prog[0], prog+1 );
		fprintf( stderr, "failed to spawn: %s: %s\n", prog[1], strerror(errno) );
		exit(1);
	}
}

FILE *create_output( int dump_only, char *ofn, char **pret, int *child_pid )
{
	FILE *output;

	if (dump_only && (ofn==NULL || streq(ofn,"-")))
		output = stdout;
	else if (dump_only && ofn)
		output = fopen( ofn, "w" );
	else
		output = prepare_output( pret, ofn, child_pid );

	if (!output)
		fprintf( stderr, "failed to create output.\n" );

	return output;
}


struct lang_def *find_lang( const char *lang_name )
{
	int i;
	for (i = 0; i < LANG_COUNT; i++)
		if (streq(lang_conf[i].name, lang_name))
			return &lang_conf[i];
	return NULL;
}

/* =======================================================

    The Main

   ======================================================= */

int main( int argc, char **argv )
{
	int dump_only = 0;
	char *qch = DEF_QCH;
	char *lang = "bash";
	char *lang_opt = NULL;
	char *lang_magic = NULL;
	char *qch_magic = NULL;
	char *out_file_name = NULL;
	int opt;
	struct lang_def *l = NULL;

	FILE *input = NULL;
	FILE *output = NULL;
	int child_pid = -1;

	char *line = NULL;
	char *tmp, *tmp2;
	size_t len_buf = 0;
	ssize_t line_len;
	int state = 0;

	while ((opt = getopt(argc, argv, "vhdl:Q:o:")) != -1) {
		switch (opt) {
			case 'd':
				dump_only = 1;
				break;
			case 'l':
				lang_opt = optarg;
				lang = lang_opt;
				break;
			case 'o':
				out_file_name = optarg;
				break;
			case 'Q':
				qch = optarg;
				break;
			case 'v':
				puts( "boa v" BOA_VERSION );
				exit(0);
			case 'h':
			default:
				fprintf( stderr, BOA_USAGE );
				exit(opt == 'h' ? 0 : 1);
		}
	}

	/* lang */
	l = find_lang( lang );
	if (!l) {
		fprintf( stderr, "unknown lang: %s\n" "available languages:\n", lang );
		for (opt = 0; opt < LANG_COUNT; opt++)
			fprintf( stderr, "\t%s\n", lang_conf[opt].name );
	}

	/* input */
	if (optind >= argc) {
		fprintf( stderr, "reading from the stdin\n" );
		input = stdin;
		setenv( "BOA_INPUT", "-", 1 );
	}
	else {
		input = fopen( argv[optind], "r" );
		if (!input) {
			fprintf( stderr, "can't open input %s\n", argv[optind] );
			exit(1);
		}
		cloexec( input );
		setenv( "BOA_INPUT", argv[optind], 1 );
	}

	/* output file */
		/* handled later, after we settle on the language */

	/* main loop */
	while ((line_len = getline(&line, &len_buf, input)) != -1) {
		line_len = chomp( line, line_len );
		/* first lines might be special */
		if (state == 0 || state == 1) {
			/* shebang? */
			if (magic( line, line_len, &lang_magic, &qch_magic )) {
				struct lang_def *tmp;
				if (lang_opt && !streq(lang_opt, lang_magic)) {
					fprintf( stderr, "magic: language: conflict with command line: `%s` vs `%s`\n", lang_opt, lang_magic );
				}
				tmp = find_lang( lang_magic );
				if (!tmp) {
					fprintf( stderr, "magic: language [%s] is not available\n", lang_magic );
					return 1;
				}
				l = tmp;
				qch = qch_magic;
				state = 2;
				continue; /* consume the line */
			}
			/* nothing special in the first line(s), go to normal processing */
			state = 2;
		}

		/* output file */
		if (!output) {
			output = create_output( dump_only, out_file_name, l->intrp, &child_pid );
			if (!output)
				return 1;
			if (l->preamble)
				fprintf( output, "%s", l->preamble );
		}

		/* the processing */
		if (state == 2) {
			if (str_begins_with2_ss( line, qch, DEF_OBR2, &tmp )) {
				/* multi-line code : pass verbatim whole block */
				fprintf( output, "%s\n", tmp );
				state = 3;
			}
			else if (str_begins_with_ss( line, qch, &tmp ) && *tmp != DEF_OBR && *tmp != qch[0]) {
				/* single-line code : pass verbatim */
				fprintf( output, "%.*s\n", (int)(line_len - (tmp-line)), tmp );
			}
			else {
				/* simple text, but probably with the expansions */
				//int c = 100;
				tmp = line;

				while (tmp < line + line_len) {
					char *p = NULL;
					tmp2 = tmp;
					//fprintf( stderr, "tmp = %p, %02x\n", tmp, (unsigned)tmp[0] );
					while (*tmp) {
						/* double qch is an escaping sequence */
						if (tmp[0] == qch[0] && tmp[1] == qch[0]) {
							//fprintf( stderr, "moving %d chars\n", (int)(1 + line_len - (tmp-line)) );
							memmove( tmp, tmp+1, line_len - (tmp-line) );
							line_len--;
							tmp++;
						}
						else if ((str_begins_with2(tmp,qch,DEF_OBR_) && (p = fcb(tmp,DEF_OBR,DEF_CBR)))) {
							break;
						}
						tmp++;
					}

					if (tmp2 != tmp) {
						/* print the textual part, with quoting */
						size_t olen = 0;
						char *p;
						p = l->quote( tmp2, tmp-tmp2, &olen );
						fprintf( output, "%s%.*s%s", l->print_s, (int)olen, p, l->print_e );
					}
					if (*tmp && p) {
						/* print from `tmp` to `p` as code */
						tmp += strlen( qch );
						tmp += 1;
						if (*tmp == '!') {
							/* simply execute, do not print */
							tmp++;
							fprintf( output, "%.*s\n", (int)(p-tmp), tmp );
						}
						else {
							/* print the code */
							fprintf( output, "%s%.*s%s", l->print_s, (int)(p-tmp), tmp, l->print_e );
						}
						tmp = p + 1;
					}
					//assert( c-- > 0 );
				}
				fprintf( output, "%s", l->print_nl );
			}
		}
		else if (state == 3) {
			/* content of the block, until the }}@ */
			if (str_begins_with2_ss( line, DEF_CBR2, qch, NULL )) {
				state = 2;
			}
			else {
				fprintf( output, "%.*s\n", (int)line_len, line );
			}
		}
	}
	free(line);

	if (!output) { /* empty input */
		output = create_output( dump_only, out_file_name, l->intrp, &child_pid );
		if (!output)
			return 1;
		if (l->preamble)
			fprintf( output, "%s", l->preamble );
	}

	if (l->closure)
		fprintf( output, "%s", l->closure );
	fflush(output);
	fclose(output);

	/* wait for the slave, if any */
	if (child_pid > 0) {
		int status = 0;
		int ret;
		do ret = waitpid( child_pid, &status, 0 ); while (ret == -1 && errno == EINTR);
		if (ret != child_pid) {
			fprintf( stderr, "something went wrong: waitpid(%d) = %d, st=%d\n", child_pid, ret, status );
			return 1;
		}
		if (status) {
			fprintf( stderr, "interpreter for %s failed! rc=%d\n", l->name, WEXITSTATUS(status) );
		}
	}

	return 0;
}
