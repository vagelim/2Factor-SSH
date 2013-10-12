#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>

/*
Build with gcc -fPIC -fno-stack-protector -lcurl -c 2fa.c
ld -lcurl -x --shared -o /lib/security/2fa.so 2fa.o
in /etc/pam.d/sshd add "auth       required     2ndfactor.so" under common-auth and change ChallengePrompt in sshd config to yes
*/


PAM_EXTERN int pam_sm_setcred( pam_handle_t *pamh, int flags, int argc, const char **argv ) {
	return PAM_SUCCESS ;
}


int converse( pam_handle_t *pamh, int nargs, struct pam_message **message, struct pam_response **response ) {
	int retval ;
	struct pam_conv *conv ;

	retval = pam_get_item( pamh, PAM_CONV, (const void **) &conv ) ; 
	if( retval==PAM_SUCCESS ) {
		retval = conv->conv( nargs, (const struct pam_message **) message, response, conv->appdata_ptr ) ;
	}

	return retval ;
}


PAM_EXTERN int pam_sm_authenticate( pam_handle_t *pamh, int flags,int argc, const char **argv ) {
	int retval ;
	int i ;
    
	char *input ;
	struct pam_message msg[1],*pmsg[1];
	struct pam_response *resp;
	
	unsigned int code_size = 5 ;
	char base_url[256] = "PLACE YOUR SCRIPT URL HERE" ;
	


	
	const char *username ;
    	if( (retval = pam_get_user(pamh,&username,"login: "))!=PAM_SUCCESS ) {
		return retval ;
	}

	/* generating a random one-time code */
	char code[code_size+1] ;
  	unsigned int seed = (unsigned int)time( NULL ) ;
  	srand( seed ) ;
	for( i=0 ; i<code_size ; i++ ) {
		int random_number = rand() % 10 ; // random int between 0 and 9
		code[i] = random_number + 48 ; // +48 transforms the int into its equivalent ascii code
	}
	code[code_size] = 0 ; // because it needs to be null terminated

	
	char url_with_params[strlen(base_url) + strlen("?username=") + strlen(username) + strlen("&code=") + code_size] ;
	strcpy( url_with_params, base_url ) ;
	strcat( url_with_params, "?username=" ) ;
	strcat( url_with_params, username ) ;
	strcat( url_with_params, "&code=" ) ;
	strcat( url_with_params, code ) ;

	/* HTTP request to service that will dispatch the code */
	CURL *curl ;
	CURLcode res;
	curl = curl_easy_init() ;
	if( curl ) {
		curl_easy_setopt( curl, CURLOPT_URL, url_with_params ) ;
		res = curl_easy_perform( curl ) ;
		curl_easy_cleanup( curl ) ;
	}

	
	pmsg[0] = &msg[0] ;
	msg[0].msg_style = PAM_PROMPT_ECHO_ON ;
	msg[0].msg = "1-time code: " ;
	resp = NULL ;
	if( (retval = converse(pamh, 1 , pmsg, &resp))!=PAM_SUCCESS ) {
		// if this function fails, make sure that ChallengeResponseAuthentication in sshd_config is set to yes
		return retval ;
	}

	
	if( resp ) {
		if( (flags & PAM_DISALLOW_NULL_AUTHTOK) && resp[0].resp == NULL ) {
	    		free( resp );
	    		return PAM_AUTH_ERR;
		}
		input = resp[ 0 ].resp;
		resp[ 0 ].resp = NULL; 		  				  
    	} else {
		return PAM_CONV_ERR;
	}
	
	
	if( strcmp(input, code)==0 ) {
		/* good to go! */
		free( input ) ;
		return PAM_SUCCESS ;
	} else {
		/* wrong code */
		free( input ) ;
		return PAM_AUTH_ERR ;
	}

}
