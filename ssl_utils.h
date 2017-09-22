/*
 * ssl_utils.h
 *
 * Functions for interacting with SSL, X509, etc.
 */
#ifndef _SSL_UTILS_H
#define _SSL_UTILS_H

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/md5.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/des.h>
#include <time.h>

#if GLOBUS
#include <globus_gsi_callback.h>
#include <globus_gsi_cert_utils.h>
#include <globus_gsi_credential.h>
#include <globus_gsi_proxy.h>
#include <globus_gsi_system_config.h>
#include <globus_gsi_system_config_constants.h>
#endif

/* EVP_MD_CTX_init() and EVP_MD_CTX_cleanup() not in OpenSSL 0.9.6. */
#if !defined(EVP_MD_CTX_FLAG_CLEANED)
#define EVP_MD_CTX_init(ctx)
#define EVP_MD_CTX_cleanup(ctx)
#define X509V3_set_nconf X509V3_set_conf_lhash
#define X509V3_EXT_add_nconf X509V3_EXT_add_conf
#endif

struct _ssl_credentials;
typedef struct _ssl_credentials SSL_CREDENTIALS;

struct _ssl_proxy_restrictions;
typedef struct _ssl_proxy_restrictions SSL_PROXY_RESTRICTIONS;

/*
 * Returns values for functions
 */
#define SSL_SUCCESS		1
#define SSL_ERROR		0

/*
 * ssl_credentials_destroy()
 *
 * Destroys the given credentials, deallocating all memory
 * associated with them.
 */
void ssl_credentials_destroy(SSL_CREDENTIALS *creds);

/*
 * ssl_proxy_file_destroy()
 *
 * Fill the proxy file with zeros and unlink.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_proxy_file_destroy(const char *path);

/*
 * ssl_certificate_load_from_file()
 *
 * Load certificate(s) from the given file into the given set of credentials.
 * Any existing certificates in the creds structure will be erased.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_certificate_load_from_file(SSL_CREDENTIALS	*creds,
				   const char		*path);

/*
 * ssl_certificate_push()
 *
 * Set given certificate as the creds' certificate, pushing any existing
 * certificate in the creds structure into the constituent certificate chain.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_certificate_push(SSL_CREDENTIALS  *creds,
                         X509 *cert);

/*
 * ssl_private_key_load_from_file()
 *
 * Load a key from the given file and store it in the given credentials
 * structure.
 * If pass_phrase_prompt is non-NULL, prompt for the
 * passphrase to be entered on the tty if needed.
 * Otherwise, if pass_phrase is non-NULL, use that passphrase
 * to decrypt the key.
 * Otherwise, assume the key is unencrypted.
 * Any existing key in the creds structure will be erased.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_private_key_load_from_file(SSL_CREDENTIALS	*creds,
				   const char		*path,
				   const char		*pass_phrase,
				   const char		*pass_phrase_prompt);

/*
 * ssl_private_key_store_to_file()
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_private_key_store_to_file(SSL_CREDENTIALS *creds,
                                  const char *path,
                                  const char *pass_phrase);


/*
 * ssl_private_key_is_encrypted()
 *
 * Returns 1 if the private key is encrypted, 0 if unencrypted, -1 on error.
 */
int ssl_private_key_is_encrypted(const char *path);

/*
 * ssl_proxy_from_pem()
 *
 * Take a buffer generated by ssl_proxy_to_pem() and return a set
 * of credentials. pass_phrase is used if needed.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_proxy_from_pem(SSL_CREDENTIALS	*creds,
		       const unsigned char	*buffer,
		       int			buffer_len,
		       const char		*pass_phrase);

/*
 * ssl_proxy_load_from_file()
 *
 * Load a proxy certificate and key from the given file, using pass_phrase
 * if needed, and storing the credentials in the given SSL_CREDENTIALS
 * structure. pass_phrase may be NULL. Any existing credentials in
 * the SSL_CREDENTIALS structure will be erased.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_proxy_load_from_file(SSL_CREDENTIALS		*creds,
			     const char			*path,
			     const char			*pass_phrase);

/*
 * ssl_proxy_to_pem()
 *
 * Return an allocated buffer with the given proxy encoded in PEM format.
 * The private key is encrypted with pass_phrase if provided (can be NULL).
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_proxy_to_pem(SSL_CREDENTIALS			*creds,
		     unsigned char			**pbuffer,
		     int				*pbuffer_len,
		     const char			*pass_phrase);

/*
 *
 * ssl_proxy_store_to_file()
 *
 * Store the the proxy in the given set of credentials to the give file.
 * The file must not exist. If pass_phrase is non-NULL it will be used
 * to encrypt the private key.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_proxy_store_to_file(SSL_CREDENTIALS		*creds,
			    const char			*path,
			    const char			*pass_phrase);

/*
 * ssl_new_credentials()
 *
 * Return a empty credentials structure for use.
 *
 * Returns NULL on error.
 */
SSL_CREDENTIALS *ssl_credentials_new();

/*
 *
 * ssl_certreq_pem_to_der()
 *
 * Given the location of a file containing a PEM certificate request
 * as input (certreq), return a DER encoded certificate request as
 * output (buffer).
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_certreq_pem_to_der(char *certreq,
                           unsigned char **buffer, int *buffer_length);

/*
 *
 * ssl_proxy_delegation_init()
 *
 * Generate a request for a proxy delegation in a buffer suitable for shipping
 * over the network.
 *
 * pcreds will be filled in with the private key and should be passed to
 * ssl_proxy_delegation_finalize() to be filled in with the returned
 * certificate.
 *
 * buffer will be set to point at an allocated buffer containing
 * data to be passed to the signer to be passed into
 * ssl_sign_proxy_request().
 *
 * buffer_length will be filled in with the length of buffer.
 *
 * requested_bits will be used as the key length for the
 * new proxy. If 0 then the length of user_certificate key
 * will be used.
 *
 * callback can point to a function that will be called
 * during key generation - see SSLeay's doc/rsa.doc
 * RSA_generate_key() function for details.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_proxy_delegation_init(SSL_CREDENTIALS	**new_creds,
			      unsigned char	**buffer,
			      int		*buffer_length,
			      int		requested_bits,
			      void		(*callback)(int,int,void *));


/*
 * ssl_proxy_delegation_finalize()
 *
 * Finalize the process of getting a proxy delegation using
 * buffers in a form suitable for shipping over the network.
 *
 * creds should be the credentials originally obtained from
 * ssl_proxy_request_init()
 *
 * buffer should be the buffer generated by ssl_proxy_request_sign().
 *
 * buffer_len should be the length of buffer.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_proxy_delegation_finalize(SSL_CREDENTIALS	*creds,
				  unsigned char		*buffer,
				  int			buffer_length);

/*
 * ssl_proxy_delegation_sign()
 *
 * Sign a proxy delegation request and generate a proxy certificate. Input and
 * output are buffers suitable for shipping over the network.
 *
 * creds contains the credentials used the sign the request.
 *
 * restrictions contains any restrictions to be placed on the
 * proxy. May be NULL in which case defaults are used.
 *
 * request_buffer contains the buffer as generated by
 * ssl_generate_proxy_request().
 *
 * request_buffer_len contains the length of request_buffer
 * in bytes.
 *
 * proxy_buffer will be filled in with a pointer to an allocated
 * buffer that contains the proxy certificate and certificate
 * chain for feeding into ssl_finish_proxy_request.
 *
 * proxy_buffer_length will be filled in to contain the length
 * of proxy_buffer.
 *
 * Returns SSL_SUCCESS or SSL_ERROR, setting verror.
 */
int ssl_proxy_delegation_sign(SSL_CREDENTIALS		*creds,
			      SSL_PROXY_RESTRICTIONS	*restrictions,
			      unsigned char		*request_buffer,
			      int			request_buffer_length,
			      unsigned char		**proxy_buffer,
			      int			*proxy_buffer_length);

/*
 * ssl_free_buffer()
 *
 * Free a buffer allocated by any of the other routines in this library.
 */
void ssl_free_buffer(unsigned char *buffer);


/*
 * ssl_proxy_restrictions_new()
 *
 * Generate a new SSL_PROXY_RESTRICTIONS object.
 *
 * Returns object on success, NULL on error setting verror.
 */
SSL_PROXY_RESTRICTIONS *ssl_proxy_restrictions_new();

/*
 * ssl_proxy_restrictions_destroy()
 *
 * Destroy a SSL_PROXY_RESTRICTIONS object, deallocating all memory
 * associated with it.
 */
void ssl_proxy_restrictions_destroy(SSL_PROXY_RESTRICTIONS *restrictions);

/*
 * ssl_proxy_restrictions_set_lifetime()
 *
 * Set the lifetime in the given SSL_PROXY_RESTRICTIONS object to
 * the given number of seconds. A values of zero for seconds means
 * to use the default.
 *
 * Returns SSL_SUCCESS on success, SSL_ERROR otherwise setting verror.
 */
int ssl_proxy_restrictions_set_lifetime(SSL_PROXY_RESTRICTIONS *restrictions,
					const long seconds);

			   
/*
 * ssl_proxy_restrictions_set_limited()
 *
 * Set whether a limited proxy should be delegated.
 * A limited flag of 1 indicates yes, a flag of 0 indicates no (default).
 *
 * Returns SSL_SUCCESS on success, SSL_ERROR otherwise setting verror.
 */
int ssl_proxy_restrictions_set_limited(SSL_PROXY_RESTRICTIONS *restrictions,
					const int limited);

			   
#if GLOBUS_TODO
/* ssl_get_base_subject_file()
 *
 * Get user's subject name from certificate in the supplied filename
 *
 * Returns 0 on success or -1 on error
 */
int
ssl_get_base_subject_file(const char *proxyfile, char **subject);

/* ssl_get_base_subject()
 *
 * Get user's subject name from SSL_CREDENTIALS.
 *
 * Returns SSL_SUCCESS or SSL_ERROR
 */
int
ssl_get_base_subject(SSL_CREDENTIALS *creds, char **subject);
#endif

/* 
 * ssl_creds_to_buffer()
 *
 * Encode credentials from SSL_CREDENTIALS struct into buffer. Memory for the 
 * buffer is obtained with malloc(3) and must be freed with free(3).
 *
 * Returns SSL_SUCCESS or SSL_ERROR
 */
int ssl_creds_to_buffer(SSL_CREDENTIALS *chain, unsigned char **buffer, 
                        int *buffer_length);

/*
 * ssl_creds_from_buffer()
 *
 * Decode credentals from buffer into SSL_CREDENTIALS struct. Caller should 
 * free *creds with ssl_credentials_destroy()
 *
 * Returns SSL_SUCCESS or SSL_ERROR
 */
int ssl_creds_from_buffer(unsigned char *buffer, int buffer_length,
                          SSL_CREDENTIALS **creds);

/*
 * ssl_creds_certificate_is_proxy()
 * 
 * Returns 1 if certificate is proxy(RFC 3820, GT3, GT2) certificate.
 *         0 if certificate is not proxy.
 *        -1 on error.
 */
int ssl_creds_certificate_is_proxy(SSL_CREDENTIALS *creds);

/*
 * ssl_sign()
 * 
 * Sign data with private key passed in SSL_CREDENTIALS. Memory for the
 * signature is allocated with malloc(3) and must be freed with free(2) when
 * no needed.
 */
int ssl_sign(unsigned char *data, int length, SSL_CREDENTIALS *creds,
             unsigned char **signature, int *signature_len);

/*
 * ssl_verify()
 *
 * Verify signature
 */
int ssl_verify(unsigned char *data, int length, SSL_CREDENTIALS *creds,
               unsigned char *signature, int signature_len);

#if GLOBUS_TODO
/*
 * int ssl_verify_gsi_chain()
 *
 * Verify that supplied chain is valid for GSI authentication.
 *
 * Returns SSL_SUCCESS or SSL_ERROR
 */
int ssl_verify_gsi_chain(SSL_CREDENTIALS *chain);

/*
 * int ssl_limited_proxy_chain()
 *
 * Return 1 if certificate chain includes a limited proxy,
 * 0 if not, -1 on error.
 */
int ssl_limited_proxy_chain(SSL_CREDENTIALS *chain);
#endif

/*
 * int ssl_limited_proxy_file()
 *
 * Return 1 if certificate chain in path includes a limited proxy,
 * 0 if not, -1 on error.
 */
int ssl_limited_proxy_file(const char path[]);

/*
 * ssl_get_times
 *
 */
int ssl_get_times(const char *proxyfile, time_t *not_before, time_t *not_after);

/*
 * ssl_error_to_verror()
 *
 * Transfer an error description out of the ssl error handler to verror.
 */
void ssl_error_to_verror();

#if GLOBUS
/*
 * globus_error_to_verror()
 *
 * Transfer an error description out of the Globus error handler to verror.
 */
void globus_error_to_verror(globus_result_t result);
#endif

/*
 * ssl_verify_cred()
 *
 * Check the validity of the credentials at the given path:
 *   - check Not Before and Not After fields against current time
 *   - check signature by trusted CA
 *   - check revocation status (CRL, OCSP)
 * Returns 0 on success, -1 on error (setting verror).
 */
int ssl_verify_cred(const char path[]);

#endif /* _SSL_UTILS_H */
