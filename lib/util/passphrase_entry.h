#ifndef _PASSPHRASE_ENTRY_H_
#define _PASSPHRASE_ENTRY_H_

/* How should we get the passphrase? */
enum passphrase_entry {
	PASSPHRASE_UNSET,
	PASSPHRASE_TTY_STDIN,
	PASSPHRASE_STDIN_ONCE,
	PASSPHRASE_TTY_ONCE,
	PASSPHRASE_ENV,
	PASSPHRASE_FILE,
};

/**
 * passphrase_entry_parse(arg, entry_method_p, entry_arg_p):
 * Parse "METHOD:ARG" from ${arg} into an ${*entry_method_p}:${*entry_arg_p}.
 */
int passphrase_entry_parse(const char *, enum passphrase_entry *,
    const char **);

/**
 * passphrase_entry_readpass(passwd, entry_method, entry_arg, once):
 * Use ${entry_method} to read a passphrase and return it as a malloced
 * NUL-terminated string via ${passwd}.  If ${entry_method} is
 * PASSPHRASE_TTY_STDIN and ${once} is zero, ask for the passphrase twice;
 * otherwise ask for it once.
 */
int passphrase_entry_readpass(char **, enum passphrase_entry, const char *,
    int);

#endif /* !_PASSPHRASE_ENTRY_H_ */
