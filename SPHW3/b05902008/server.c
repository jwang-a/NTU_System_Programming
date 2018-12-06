/*b05902008王行健*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>

#define TIMEOUT_SEC 5		// timeout in seconds for wait for a connection 
#define MAXBUFSIZE  1024	// timeout in seconds for wait for a connection 
#define NO_USE      0		// status of a http request
#define ERROR	    -1	
#define READING     1		
#define WRITING     2		
#define ERR_EXIT(a) { perror(a); exit(1); }

typedef struct {
    char hostname[512];		// hostname
    unsigned short port;	// port to listen
    int listen_fd;		// fd to wait for a new connection
} http_server;

typedef struct {
    int conn_fd;		// fd to talk with client
    int status;			// not used, error, reading (from client)
                                // writing (to client)
    char file[MAXBUFSIZE];	// requested file
    char query[MAXBUFSIZE];	// requested query
    char host[MAXBUFSIZE];	// client host
    char *buf;			// data sent by/to client
    size_t buf_len;		// bytes used by buf
    size_t buf_size; 		// bytes allocated for buf
    size_t buf_idx; 		// offset for reading and writing
} http_request;

typedef struct {
    char c_time_string[100];
} TimeInfo;

static char* logfilenameP;	// log file name


// Forwards
//
static void init_http_server( http_server *svrP,  unsigned short port );
// initailize a http_request instance, exit for error

static void init_request( http_request* reqP );
// initailize a http_request instance

static void free_request( http_request* reqP );
// free resources used by a http_request instance

static int read_header_and_file( http_request* reqP, int *errP);
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: continue to it until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected

static void set_ndelay( int fd );
// Set NDELAY mode on a socket.

static void add_to_buf( http_request *reqP, char* str, size_t len );
static void strdecode( char* to, char* from );
static int hexit( char c );
static char* get_request_line( http_request *reqP );
static void* e_malloc( size_t size );
static void* e_realloc( void* optr, size_t size );

int maxfd;
int list[6000000];
int pipefd[6000000][2];
int pid[6000000];
int connpid[6000000];
int died,NEW = 1; 
char laexit[1000];
http_request* requestP = NULL;// pointer to http requests from client

void sig_chld(int signo, siginfo_t *cinfo,void* context){
  int conn_fd;
  conn_fd = connpid[cinfo->si_pid];
  if(list[conn_fd]<0)
    return;
  NEW = 0;
  if(cinfo->si_status!=0){
	int buflen,conlen;
	struct stat sb;
	time_t now;
	char buf[65536],timebuf[100],conbuf[65536];
	if(cinfo->si_status==1){
		buflen = snprintf(buf,sizeof(buf),"HTTP/1.1 404 Not Found\015\012Server: SP TOY\015\012");
		add_to_buf(&requestP[conn_fd],buf,buflen);
	}
	else if(cinfo->si_status==2){
		buflen = snprintf(buf,sizeof(buf),"HTTP/1.1 400 Bad Request\015\012Server: SP TOY\015\012");
		add_to_buf(&requestP[conn_fd],buf,buflen);
	}
	now = time((time_t*)0);
	(void) strftime(timebuf,sizeof(timebuf),"%a, %d %b %Y %H:%M:%S GMT",gmtime(&now));
	buflen = snprintf(buf,sizeof(buf),"Date: %s\015\012",timebuf);
	add_to_buf(&requestP[conn_fd],buf,buflen);
	conlen = read(pipefd[conn_fd][1],conbuf,sizeof(conbuf));
	buflen = snprintf(buf,sizeof(buf),"Content-Length: %d\015\012", conlen);
	add_to_buf(&requestP[conn_fd],buf,buflen);
	buflen = snprintf(buf,sizeof(buf),"Connection: close\015\012\015\012");	
	add_to_buf(&requestP[conn_fd],buf,buflen);
	add_to_buf(&requestP[conn_fd],conbuf,conlen);
	fprintf(stderr,"Error\n");
	send(conn_fd,requestP[conn_fd].buf,requestP[conn_fd].buf_len,0);
	fprintf(stderr,"connection closed\n");
	if(strcmp("info",requestP[conn_fd].file)!=0){
		time_t curt;
		time(&curt);
                char tb[1000];
		sprintf(tb,"%s",ctime(&curt));
		tb[strlen(tb)-1] = '\0';
		sprintf(laexit,"Last Exit CGI: %s, ,CGIname:%s, Filename:%s",tb,requestP[conn_fd].file,&requestP[conn_fd].query[9]);
	}
	died++;
	close(requestP[conn_fd].conn_fd);
        close(pipefd[conn_fd][0]);
        close(pipefd[conn_fd][1]);
        free_request(&requestP[conn_fd]);
	list[conn_fd] = 0;
        waitpid(cinfo->si_pid,NULL,0);
  }
  else{
	int buflen,conlen;
	struct stat sb;
	time_t now;
	char buf[65536],timebuf[100],conbuf[65536];
	stat(requestP[conn_fd].file,&sb);
        buflen = snprintf(buf,sizeof(buf),"HTTP/1.1 200 OK\015\012Server: SP TOY\015\012");
        add_to_buf(&requestP[conn_fd],buf,buflen);
     	now = time((time_t*)0);
        (void) strftime(timebuf,sizeof(timebuf),"%a, %d %b %Y %H:%M:%S GMT",gmtime(&now));
        buflen = snprintf(buf,sizeof(buf),"Date: %s\015\012",timebuf);
        add_to_buf(&requestP[conn_fd],buf,buflen);
	conlen = read(pipefd[conn_fd][0],conbuf,sizeof(conbuf));
	buflen = snprintf(buf,sizeof(buf),"Content-Length: %d\015\012", conlen);
        add_to_buf(&requestP[conn_fd],buf,buflen);
       	buflen = snprintf(buf,sizeof(buf),"Connection: close\015\012\015\012");	
        add_to_buf(&requestP[conn_fd],buf,buflen);
        add_to_buf(&requestP[conn_fd],conbuf,conlen);
        // ready for writing
	fprintf(stderr,"writing (buf %p, idx %d) %d bytes to request fd %d\n",requestP[conn_fd].buf,(int) requestP[conn_fd].buf_idx,(int)requestP[conn_fd].buf_len,requestP[conn_fd].conn_fd);
	// write once only and ignore error
	send(conn_fd,requestP[conn_fd].buf,requestP[conn_fd].buf_len,0);
        //nwritten = write(requestP[conn_fd].conn_fd,requestP[conn_fd].buf,requestP[conn_fd].buf_len);
        fprintf(stderr,"complete writing on fd %d\n",requestP[conn_fd].conn_fd);
	fprintf(stderr,"connection closed\n");
	if(strcmp("info",requestP[conn_fd].file)!=0){
		time_t curt;
		time(&curt);
               	char tb[1000];
		sprintf(tb,"%s",ctime(&curt));
		tb[strlen(tb)-1] = '\0';
		sprintf(laexit,"Last Exit CGI: %s, CGIname:%s, Filename:%s",tb,requestP[conn_fd].file,&requestP[conn_fd].query[9]);
	}
	died++;
        close(requestP[conn_fd].conn_fd);
        close(pipefd[conn_fd][0]);
        close(pipefd[conn_fd][1]);
        free_request(&requestP[conn_fd]);
	list[conn_fd] = 0;
	list[pipefd[conn_fd][0]] = 0;
        waitpid(cinfo->si_pid,NULL,0);
        }
  return;
}

void sig_usr1(int signo,siginfo_t *cinfo,void *context){
  TimeInfo *p_map;
  char buf[65536],conbuf[65536],rbuf[65536],timebuf[65536];
  int buflen,conlen,conn_fd,rlen = 0;
  time_t now;
  for(int i = 4;i<maxfd;i++)
    if(list[i]==1){
	sprintf(&rbuf[rlen]," %d",pid[i]);
	rlen = strlen(buf);
    }
  sprintf(&rbuf[rlen]," %d",getpid());
puts("...");
  if(NEW==0){  
    int fd = open("time_log", O_RDONLY);
    printf("%d\n",fd);
    p_map = (TimeInfo*)mmap(0, sizeof(TimeInfo),  PROT_READ,  MAP_SHARED, fd, 0);
puts("...");
puts(p_map->c_time_string);
    int oc = strstr(p_map->c_time_string,"\n")-p_map->c_time_string;
printf("%d\n",oc);
    strncpy(laexit,p_map->c_time_string,oc);
puts("...");
    laexit[oc] = '\0';
    close(fd);
  }
puts("...");
  conlen = snprintf(conbuf,sizeof(conbuf),"%d processes died previously.\nPIDs of Running Processes:%s\n%s",died,rbuf,laexit);
  conn_fd = connpid[cinfo->si_pid];
  buflen = snprintf(buf,sizeof(buf),"HTTP/1.1 200 OK\015\012Server: SP TOY\015\012");
  add_to_buf(&requestP[conn_fd],buf,buflen);
  now = time((time_t*)0);
  (void) strftime(timebuf,sizeof(timebuf),"%a, %d %b %Y %H:%M:%S GMT",gmtime(&now));
  buflen = snprintf(buf,sizeof(buf),"Date: %s\015\012",timebuf);
  add_to_buf(&requestP[conn_fd],buf,buflen);
  buflen = snprintf(buf,sizeof(buf),"Content-Length: %d\015\012", conlen);
  add_to_buf(&requestP[conn_fd],buf,buflen);
  buflen = snprintf(buf,sizeof(buf),"Connection: close\015\012\015\012");	
  add_to_buf(&requestP[conn_fd],buf,buflen);
  add_to_buf(&requestP[conn_fd],conbuf,conlen);
  fprintf(stderr,"INFO\n");
  send(conn_fd,requestP[conn_fd].buf,requestP[conn_fd].buf_len,0);
  fprintf(stderr,"connection closed\n");
  died++;
  close(requestP[conn_fd].conn_fd);
  close(pipefd[conn_fd][0]);
  close(pipefd[conn_fd][1]);
  free_request(&requestP[conn_fd]);
  list[conn_fd] = -1;
  waitpid(cinfo->si_pid,NULL,0);
puts("ENDIN");
}

int main( int argc, char** argv ) {
    struct sigaction CHLD,USR1;
    sigset_t mask,block,in,noth;
    sigemptyset(&noth); 
    sigemptyset(&in); 
    sigemptyset(&mask);
    sigaddset(&mask,SIGCHLD);
    sigaddset(&in,SIGCHLD);
    sigaddset(&mask,SIGUSR1);
    CHLD.sa_flags = SA_SIGINFO;
    CHLD.sa_flags = SA_SIGINFO;
    CHLD.sa_mask = mask;
    USR1.sa_mask = mask;
    CHLD.sa_sigaction = sig_chld;
    USR1.sa_sigaction = sig_usr1;
    sigaction(SIGCHLD,&CHLD,NULL);
    sigaction(SIGUSR1,&USR1,NULL);
    http_server server;		// http server


    struct sockaddr_in cliaddr; // used by accept()
    int clilen;

    int conn_fd;		// fd for a new connection with client
    int err;			// used by read_header_and_file()
    int i, ret, nwritten;
    

    // Parse args. 
    if ( argc != 3 ) {
        (void) fprintf( stderr, "usage:  %s port# logfile\n", argv[0] );
        exit( 1 );
    }

    logfilenameP = argv[2];

    // Initialize http server
    init_http_server( &server, (unsigned short) atoi( argv[1] ) );

    maxfd = getdtablesize();
    requestP = ( http_request* ) malloc( sizeof( http_request ) * maxfd );
    if ( requestP == (http_request*) 0 ) {
	fprintf( stderr, "out of memory allocating all http requests\n" );
	exit( 1 );
    }
    for ( i = 0; i < maxfd; i ++ )
        init_request( &requestP[i] );
    requestP[ server.listen_fd ].conn_fd = server.listen_fd;
    requestP[ server.listen_fd ].status = READING;

    fprintf( stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d, logfile %s...\n", server.hostname, server.port, server.listen_fd, maxfd, logfilenameP );

    died = 0;
    memset(list,0,sizeof(int)*maxfd);
    list[3] = 1;
    fd_set doset;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    sprintf(laexit,"Last Exit CGI: None, CGIname: None, Filename: None");
    // Main loop. 
    while (1) {
	// Wait for a connection.
	sigprocmask(SIG_SETMASK,&noth,NULL);
	sigprocmask(SIG_SETMASK,&in,NULL);
	clilen = sizeof(cliaddr);

        FD_ZERO(&doset);
	for(int i = 0;i<maxfd;i++)
	    if(list[i]!=0)
		FD_SET(i,&doset);
	select(maxfd,&doset,NULL,NULL,&timeout);

        if(FD_ISSET(3,&doset)){
		conn_fd = accept( server.listen_fd, (struct sockaddr *) &cliaddr, (socklen_t *) &clilen );
		if ( conn_fd < 0 ) {
		    if ( errno == EINTR || errno == EAGAIN ) continue; // try again 
		    if ( errno == ENFILE ) {
	       		 (void) fprintf( stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd );
		        continue;
        	    }	
		    ERR_EXIT( "accept" )
		}
		list[conn_fd] = 1;
	        requestP[conn_fd].conn_fd = conn_fd;
	        requestP[conn_fd].status = READING;		
		strcpy( requestP[conn_fd].host, inet_ntoa( cliaddr.sin_addr ) );
	        set_ndelay( conn_fd );
	        fprintf( stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host );
	}
        conn_fd = -1;
	for(int i = 4;i<maxfd;i++){
          if(list[i]<=0 || FD_ISSET(i,&doset)==0)
            continue;
          conn_fd = i;
        }
        if(conn_fd==-1)
          continue;
	while (1) {
	    ret = read_header_and_file( &requestP[conn_fd], &err);
	    if ( ret > 0 ) continue;
	    else if ( ret < 0 ) {
	        // error for reading http header or requested file
                fprintf( stderr, "error on fd %d, code %d\n", requestP[conn_fd].conn_fd, err );
                requestP[conn_fd].status = ERROR;
	        close( requestP[conn_fd].conn_fd );
	        free_request( &requestP[conn_fd] );
                break;
	    }
            else if ( ret == 0 ) {
printf("%s\n",requestP[conn_fd].file);
printf("%s\n",requestP[conn_fd].query);
		if(strcmp(requestP[conn_fd].file,"info")==0 && requestP[conn_fd].query[0]=='\0'){
		  puts("!!!");
                  goto FORK;
		}
                int length = strlen(requestP[conn_fd].file);
                for(int i = 0;i<length;i++)
                  if(!isdigit(requestP[conn_fd].file[i])&&!isalpha(requestP[conn_fd].file[i])&&requestP[conn_fd].file[i]!='_'){
                    goto B404;
                  }
                goto NB404;
                B404:;
		free(requestP[conn_fd].buf);
		requestP[conn_fd].buf = NULL;
		requestP[conn_fd].buf_size = 0;
		requestP[conn_fd].buf_len = 0;
		requestP[conn_fd].buf_idx = 0;
		int buflen,conlen;
		struct stat sb;
		time_t now;
		char buf[65536],timebuf[100],conbuf[65536];
		stat(requestP[conn_fd].file,&sb);
		buflen = snprintf(buf,sizeof(buf),"HTTP/1.1 400 Bad Request\015\012Server: SP TOY\015\012");
		add_to_buf(&requestP[conn_fd],buf,buflen);
		now = time((time_t*)0);
		(void) strftime(timebuf,sizeof(timebuf),"%a, %d %b %Y %H:%M:%S GMT",gmtime(&now));
		buflen = snprintf(buf,sizeof(buf),"Date: %s\015\012",timebuf);
		add_to_buf(&requestP[conn_fd],buf,buflen);
		buflen = snprintf(buf,sizeof(buf),"Content-Length: %d\015\012", 68);
		add_to_buf(&requestP[conn_fd],buf,buflen);
		buflen = snprintf(buf,sizeof(buf),"Connection: close\015\012\015\012");	
		add_to_buf(&requestP[conn_fd],buf,buflen);
		add_to_buf(&requestP[conn_fd],"400 BAD REQUEST\nONLY 'a'~'z' 'A'~'Z' '0'~'9' '_' ALLOWED IN CGI NAME\0",68);
		fprintf(stderr,"400 BAD REQUEST\n");
		send(conn_fd,requestP[conn_fd].buf,requestP[conn_fd].buf_len,0);
		fprintf(stderr,"connection closed\n");
		close(conn_fd);
		free_request(&requestP[conn_fd]);
		list[conn_fd] = 0;
		goto END;
                NB404:;
		if(access(requestP[conn_fd].file,F_OK)==-1){
			free(requestP[conn_fd].buf);
			requestP[conn_fd].buf = NULL;
			requestP[conn_fd].buf_size = 0;
			requestP[conn_fd].buf_len = 0;
			requestP[conn_fd].buf_idx = 0;
			int buflen,conlen;
			struct stat sb;
			time_t now;
			char buf[65536],timebuf[100],conbuf[65536];
			stat(requestP[conn_fd].file,&sb);
			buflen = snprintf(buf,sizeof(buf),"HTTP/1.1 404 Not Found\015\012Server: SP TOY\015\012");
			add_to_buf(&requestP[conn_fd],buf,buflen);
			now = time((time_t*)0);
			(void) strftime(timebuf,sizeof(timebuf),"%a, %d %b %Y %H:%M:%S GMT",gmtime(&now));
			buflen = snprintf(buf,sizeof(buf),"Date: %s\015\012",timebuf);
			add_to_buf(&requestP[conn_fd],buf,buflen);
			buflen = snprintf(buf,sizeof(buf),"Content-Length: %d\015\012",32);
			add_to_buf(&requestP[conn_fd],buf,buflen);
			buflen = snprintf(buf,sizeof(buf),"Connection: close\015\012\015\012");	
			add_to_buf(&requestP[conn_fd],buf,buflen);
			add_to_buf(&requestP[conn_fd],"404 NOT FOUND\nCGI DOES NOT EXIST\0",32);
			fprintf(stderr,"404 NOT FOUND\n");
			send(conn_fd,requestP[conn_fd].buf,requestP[conn_fd].buf_len,0);
			fprintf(stderr,"connection closed\n");
			close(conn_fd);
			free_request(&requestP[conn_fd]);
			list[conn_fd] = 0;
			goto END;
		}
		FORK:;
                int fd[3][2];
                pipe(fd[0]);
                pipe(fd[1]);
                pipe(fd[2]);
                if((pid[conn_fd] = fork())==0){
                  close(fd[0][0]);
                  close(fd[1][1]);
                  close(fd[2][0]);
                  dup2(fd[1][0],STDIN_FILENO);
                  dup2(fd[0][1],STDOUT_FILENO);
                  dup2(fd[2][1],STDERR_FILENO);
                  execl(requestP[conn_fd].file,requestP[conn_fd].file,(char *)0);
                }
		free(requestP[conn_fd].buf);
		requestP[conn_fd].buf = NULL;
	        requestP[conn_fd].buf_size = 0;
	        requestP[conn_fd].buf_len = 0;
	        requestP[conn_fd].buf_idx = 0;
		connpid[pid[conn_fd]] = conn_fd;
                close(fd[1][0]);
		close(fd[0][1]);
		close(fd[2][1]);
		pipefd[conn_fd][0] = fd[0][0];
		pipefd[conn_fd][1] = fd[2][0];
		write(fd[1][1],&requestP[conn_fd].query[9],strlen(&requestP[conn_fd].query[9]));
		close(fd[1][1]);
		END:;
                break;
	    }
	}
    }
    free( requestP );
    return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working

static void init_request( http_request* reqP ) {
    reqP->conn_fd = -1;
    reqP->status = 0;		// not used
    reqP->file[0] = (char) 0;
    reqP->query[0] = (char) 0;
    reqP->host[0] = (char) 0;
    reqP->buf = NULL;
    reqP->buf_size = 0;
    reqP->buf_len = 0;
    reqP->buf_idx = 0;
}

static void free_request( http_request* reqP ) {
    if ( reqP->buf != NULL ) {
	free( reqP->buf );
	reqP->buf = NULL;
    }
    init_request( reqP );
}


#define ERR_RET( error ) { *errP = error; return -1; }
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: read more, continue until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected
//
static int read_header_and_file( http_request* reqP, int *errP) {
    // Request variables
    char* file = (char *) 0;
    char* path = (char *) 0;
    char* query = (char *) 0;
    char* protocol = (char *) 0;
    char* method_str = (char *) 0;
    int r, fd;
    struct stat sb;
    char timebuf[100];
    int buflen;
    char buf[65536];
    time_t now;
    void *ptr;

    // Read in request from client
    while (1) {
	r = read( reqP->conn_fd, buf, sizeof(buf) );
	if ( r < 0 && ( errno == EINTR || errno == EAGAIN ) ) return 1;
	if ( r <= 0 ) ERR_RET( 1 )
	add_to_buf( reqP, buf, r );
	if ( strstr( reqP->buf, "\015\012\015\012" ) != (char*) 0 ||
	     strstr( reqP->buf, "\012\012" ) != (char*) 0 ) break;
    }
    //fprintf( stderr, "header: %s\n", reqP->buf );
    // Parse the first line of the request.
    method_str = get_request_line( reqP );
    if ( method_str == (char*) 0 ) ERR_RET( 2 )
    path = strpbrk( method_str, " \t\012\015" );
    if ( path == (char*) 0 ) ERR_RET( 2 )
    *path++ = '\0';
    path += strspn( path, " \t\012\015" );
    protocol = strpbrk( path, " \t\012\015" );
    if ( protocol == (char*) 0 ) ERR_RET( 2 )
    *protocol++ = '\0';
    protocol += strspn( protocol, " \t\012\015" );
    query = strchr( path, '?' );
    if ( query == (char*) 0 )
	query = "";
    else
	*query++ = '\0';
    if ( strcasecmp( method_str, "GET" ) != 0 ) ERR_RET( 3 )
    else {
        strdecode( path, path );
        if ( path[0] != '/' ) ERR_RET( 4 )
	else file = &(path[1]);
    }

    if ( strlen( file ) >= MAXBUFSIZE-1 ) ERR_RET( 4 )
    if ( strlen( query ) >= MAXBUFSIZE-1 ) ERR_RET( 5 )

    strcpy( reqP->file, file );
    strcpy( reqP->query, query );

    
    /*
    if ( query[0] == (char) 0 ) {
        // for file request, read it in buf
        r = stat( reqP->file, &sb );
        if ( r < 0 ) ERR_RET( 6 )

        fd = open( reqP->file, O_RDONLY );
        if ( fd < 0 ) ERR_RET( 7 )

	reqP->buf_len = 0;

        buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012" );
        add_to_buf( reqP, buf, buflen );
     	now = time( (time_t*) 0 );
        (void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
        buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
        add_to_buf( reqP, buf, buflen );
	buflen = snprintf(
	    buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t) sb.st_size );
        add_to_buf( reqP, buf, buflen );
        buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
        add_to_buf( reqP, buf, buflen );

	ptr = mmap( 0, (size_t) sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
	if ( ptr == (void*) -1 ) ERR_RET( 8 )
        add_to_buf( reqP, ptr, sb.st_size );
	(void) munmap( ptr, sb.st_size );
	close( fd );
	// printf( "%s\n", reqP->buf );
	// fflush( stdout );
	reqP->buf_idx = 0; // writing from offset 0
	return 0;
    }
    */

    return 0;
}


static void add_to_buf( http_request *reqP, char* str, size_t len ) { 
    char** bufP = &(reqP->buf);
    size_t* bufsizeP = &(reqP->buf_size);
    size_t* buflenP = &(reqP->buf_len);

    if ( *bufsizeP == 0 ) {
	*bufsizeP = len + 500;
	*buflenP = 0;
	*bufP = (char*) e_malloc( *bufsizeP );
    } else if ( *buflenP + len >= *bufsizeP ) {
	*bufsizeP = *buflenP + len + 500;
	*bufP = (char*) e_realloc( (void*) *bufP, *bufsizeP );
    }
    (void) memmove( &((*bufP)[*buflenP]), str, len );
    *buflenP += len;
    (*bufP)[*buflenP] = '\0';
}

static char* get_request_line( http_request *reqP ) { 
    int begin;
    char c;

    char *bufP = reqP->buf;
    int buf_len = reqP->buf_len;

    for ( begin = reqP->buf_idx ; reqP->buf_idx < buf_len; ++reqP->buf_idx ) {
	c = bufP[ reqP->buf_idx ];
	if ( c == '\012' || c == '\015' ) {
	    bufP[reqP->buf_idx] = '\0';
	    ++reqP->buf_idx;
	    if ( c == '\015' && reqP->buf_idx < buf_len && 
	        bufP[reqP->buf_idx] == '\012' ) {
		bufP[reqP->buf_idx] = '\0';
		++reqP->buf_idx;
	    }
	    return &(bufP[begin]);
	}
    }
    fprintf( stderr, "http request format error\n" );
    exit( 1 );
}



static void init_http_server( http_server *svrP, unsigned short port ) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname( svrP->hostname, sizeof( svrP->hostname) );
    svrP->port = port;
   
    svrP->listen_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( svrP->listen_fd < 0 ) ERR_EXIT( "socket" )

    bzero( &servaddr, sizeof(servaddr) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( port );
    tmp = 1;
    if ( setsockopt( svrP->listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &tmp, sizeof(tmp) ) < 0 ) 
	ERR_EXIT ( "setsockopt " )
    if ( bind( svrP->listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr) ) < 0 ) ERR_EXIT( "bind" )

    if ( listen( svrP->listen_fd, 1024 ) < 0 ) ERR_EXIT( "listen" )
}

// Set NDELAY mode on a socket.
static void set_ndelay( int fd ) {
    int flags, newflags;

    flags = fcntl( fd, F_GETFL, 0 );
    if ( flags != -1 ) {
	newflags = flags | (int) O_NDELAY; // nonblocking mode
	if ( newflags != flags )
	    (void) fcntl( fd, F_SETFL, newflags );
    }
}   

static void strdecode( char* to, char* from ) {
    for ( ; *from != '\0'; ++to, ++from ) {
	if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) ) {
	    *to = hexit( from[1] ) * 16 + hexit( from[2] );
	    from += 2;
	} else {
	    *to = *from;
        }
    }
    *to = '\0';
}


static int hexit( char c ) {
    if ( c >= '0' && c <= '9' )
	return c - '0';
    if ( c >= 'a' && c <= 'f' )
	return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' )
	return c - 'A' + 10;
    return 0;           // shouldn't happen
}


static void* e_malloc( size_t size ) {
    void* ptr;

    ptr = malloc( size );
    if ( ptr == (void*) 0 ) {
	(void) fprintf( stderr, "out of memory\n" );
	exit( 1 );
    }
    return ptr;
}


static void* e_realloc( void* optr, size_t size ) {
    void* ptr;

    ptr = realloc( optr, size );
    if ( ptr == (void*) 0 ) {
	(void) fprintf( stderr, "out of memory\n" );
	exit( 1 );
    }
    return ptr;
}
