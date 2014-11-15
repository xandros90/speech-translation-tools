///-----------------------------------------------------------------
///
/// @file      ClientTsFrm.cpp
/// @author    Utente
/// Created:   07/10/2014 15:14:10
/// @section   DESCRIPTION
///            ClientTsFrm class implementation
///
///------------------------------------------------------------------

#include "ClientTsFrm.h"
#include "Login.h"
#include "utility.h"
#include <windows.h>
#include <atlbase.h>
#include <sphelper.h>
#include <sapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <time.h>
#include <limits.h>
#include <audiere.h>
#include <irrKlang.h>
#include <string.h>
//#include <winsock2.h>
#include <iostream>
#include <public_definitions.h>
#include <public_errors.h>
#include <clientlib_publicdefinitions.h>
#include <clientlib.h>
#include <wx/msgdlg.h>
#include <wx/sstream.h>
#include <wx/protocol/http.h>
#include "rapidjson/document.h"		// rapidjson's DOM-style API
#include "rapidjson/prettywriter.h"	// for stringify JSON
#include "rapidjson/filestream.h"

#define MAX 30

#define DEFAULT_VIRTUAL_SERVER 1
#define NAME_BUFSIZE 1024

#define CHECK_ERROR(x) if((error = x) != ERROR_ok) { goto on_error; }
#define IDENTITY_BUFSIZE 1024

#ifdef _WIN32
#define snprintf sprintf_s
#define SLEEP(x) Sleep(x)
#else
#define SLEEP(x) usleep(x*1000)
#endif

using namespace audiere;
using namespace std;
using namespace ATL;
using namespace rapidjson;
using namespace irrklang;
struct WaveHeader {
	/* Riff chunk */
	char riffId[4];
	unsigned int len;
	char riffType[4];

	/* Format chunk */
	char fmtId[4];  // 'fmt '
	unsigned int fmtLen;
	unsigned short formatTag;
	unsigned short channels;
	unsigned int samplesPerSec;
	unsigned int avgBytesPerSec;
	unsigned short blockAlign;
	unsigned short bitsPerSample;

	/* Data chunk */
	char dataId[4];  // 'data'
	unsigned int dataLen;
};

typedef struct colore
{
    unsigned short red;
    unsigned short green;
    unsigned short blue;
} COLORE;

struct user
{
    wxString nome;
    unsigned short colore;
    unsigned short usato;
};

struct stringa {
  char *ptr;
  size_t len;
};

typedef struct message
{
    wxString nick;
    wxString msg;
    wxString lang;
} MESSAGE;

typedef struct header_file
{
	    char chunk_id[4];
	    int chunk_size;
	    char format[4];
		    char subchunk1_id[4];
		    int subchunk1_size;
	    short int audio_format;
		    short int num_channels;
		    int sample_rate;            // sample_rate denotes the sampling rate.
		    int byte_rate;
		    short int block_align;
		    short int bits_per_sample;
		    char subchunk2_id[4];
		    int subchunk2_size;         // subchunk2_size denotes the number of samples.
} header;

typedef struct header_file* header_p;

struct WriteThis {
	const char *readptr;
	long sizeleft;
};

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	struct WriteThis *pooh = (struct WriteThis *)userp;

	if (size*nmemb < 1)
		return 0;

	if (pooh->sizeleft) {
		*(char *)ptr = pooh->readptr[0]; /* copy one single byte */
		pooh->readptr++;                 /* advance pointer */
		pooh->sizeleft--;                /* less data left */
		return 1;                        /* we return 1 byte at a time! */
	}

	return 0;                          /* no more data left to deliver */
}

    DWORD myThreadID;
    DWORD myThreadID2;
	WSADATA wsadata;
	SOCKET sock;
	SOCKADDR_IN client_addr;
	
	int iresult;
	int numero;
	int max;
	short flag = 0;
	
	wxString strGlobale="";
	wxString oldstrGlobale="";
	wxString strNick="";
	wxString strMessage="";
	wxString nomeClient[MAX];
	wxDateTime data;
	wxString StringTranslate="";
	wxString TESTTTS="";
	char **nomeclient;
	char SERVER_ADDRESS[20];
    char NICK[50];
    char LINGUA[20];
    char SERVIZIO[20];
    char LINGUA_MSG_SRC[20]={""};
    char MSG_SRC[50]={""};
    char API_KEY[50]={""};
    char url[256]={""};
    char MSG_PARSE[1024]={""};
	char traduzione_jar[512] = { "" };
    int PORT=9987;
    int cmbel=0;
    COLORE colori[10];
    unsigned conta_client;
    unsigned short set_color_client;
    unsigned short coloreClient[MAX];
    struct user persona[MAX];
	ISoundEngine* engine;
	IAudioRecorder* recorder;
	bool sound_flag = false;


	void writeWaveFile(const char* filename, SAudioStreamFormat format, void* data)
	{
		if (!data)
		{
			printf("Could not save recorded data to %s, nothing recorded\n", filename);
			return;
		}

		FILE* file = fopen(filename, "wb");

		if (file)
		{
			// write wave header 
			unsigned short formatType = 1;
			unsigned short numChannels = format.ChannelCount;
			unsigned long  sampleRate = format.SampleRate;
			unsigned short bitsPerChannel = format.getSampleSize() * 8;
			unsigned short bytesPerSample = format.getFrameSize();
			unsigned long  bytesPerSecond = format.getBytesPerSecond();
			unsigned long  dataLen = format.getSampleDataSize();

			const int fmtChunkLen = 16;
			const int waveHeaderLen = 4 + 8 + fmtChunkLen + 8;

			unsigned long totalLen = waveHeaderLen + dataLen;

			fwrite("RIFF", 4, 1, file);
			fwrite(&totalLen, 4, 1, file);
			fwrite("WAVE", 4, 1, file);
			fwrite("fmt ", 4, 1, file);
			fwrite(&fmtChunkLen, 4, 1, file);
			fwrite(&formatType, 2, 1, file);
			fwrite(&numChannels, 2, 1, file);
			fwrite(&sampleRate, 4, 1, file);
			fwrite(&bytesPerSecond, 4, 1, file);
			fwrite(&bytesPerSample, 2, 1, file);
			fwrite(&bitsPerChannel, 2, 1, file);

			// write data

			fwrite("data", 4, 1, file);
			fwrite(&dataLen, 4, 1, file);
			fwrite(data, dataLen, 1, file);

			// finish

			printf("Saved audio as %s\n", filename);
			fclose(file);
		}
		else
			printf("Could not open %s to write audio data\n", filename);
	}

	std::string hex(unsigned int c)
	{
		std::ostringstream stm;
		stm << '%' << std::hex << std::uppercase << c;
		return stm.str();
	}

	std::string url_encode(const std::string& str)
	{
		static const std::string unreserved = "0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"-_.~";
		std::string result;

		for (unsigned char c : str)
		{
			if (unreserved.find(c) != std::string::npos) result += c;
			else result += hex(c);
		}

		return result;
	}

int JSON()
{
    const char json[2048*2]={""};
	FILE * jfile;
	if (jfile = fopen("..\\conf\\pagina.htm", "r"))
	{
		fscanf(jfile, "%s", &json);
		fflush(jfile);
		fclose(jfile);
	}
	Document document;	// Default template parameter uses UTF8 and MemoryPoolAllocator.

#if 0
	// "normal" parsing, decode strings to new buffers. Can use other input stream via ParseStream().
	if (document.Parse<0>(json).HasParseError())
		return 1;
#else
	// In-situ parsing, decode strings directly in the source string. Source must be string.
	char buffer[sizeof(json)];
	memcpy(buffer, json, sizeof(json));
	if (document.ParseInsitu<0>(buffer).HasParseError())
		return 1;
#endif

	//wxMessageBox("\nParsing to document succeeded.\n");

	////////////////////////////////////////////////////////////////////////////
	// 2. Access values in document. 

	printf("\nAccess values in document:\n");
	assert(document.IsObject());	// Document is a JSON value represents the root of DOM. Root can be either an object or array.

	assert(document.HasMember("token_type"));
	assert(document["token_type"].IsString());
	printf("hello = %s\n", document["token_type"].GetString());
	
	assert(document.HasMember("access_token"));
	assert(document["access_token"].IsString());
	printf("hello = %s\n", document["access_token"].GetString());
	
	assert(document.HasMember("expires_in"));
	assert(document["expires_in"].IsString());
	printf("hello = %s\n", document["expires_in"].GetString());
	
	assert(document.HasMember("scope"));
	assert(document["scope"].IsString());
	printf("hello = %s\n", document["scope"].GetString());
	
	FILE*js;
	if (js = fopen("..\\conf\\JSON.txt", "w"))
	{
		//fprintf(js,"%s\n",document["token_type"].GetString());
		fprintf(js, "%s", document["access_token"].GetString());
		//fprintf(js,"%s\n",document["expires_in"].GetString());
		//fprintf(js,"%s\n",document["scope"].GetString());
		fclose(js);
	}
	//return document["access_token"].GetString();
}
void SetupColor()
{
    colori[0].red=255;
    colori[0].green=0;
    colori[0].blue=0;
    
    colori[1].red=0;
    colori[1].green=255;
    colori[1].blue=0;
    
    colori[2].red=0;
    colori[2].green=0;
    colori[2].blue=255;
}
void init_string(struct stringa *s) {
  s->len = 0;
  s->ptr = (char*)malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct stringa *s)
{
  size_t new_len = s->len + size*nmemb;
  s->ptr = (char*)realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(s->ptr+s->len, ptr, size*nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size*nmemb;
}

/*static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t retcode;
	curl_off_t nread;

	intptr_t fd = (intptr_t)stream;

	retcode = read(fd, ptr, size * nmemb);

	nread = (curl_off_t)retcode;

	fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T
		" bytes from file\n", nread);

	return retcode;
}*/

void parseBing(char *parola)
{
	/*wchar_t* wString = new wchar_t[4096];
	MultiByteToWideChar(CP_ACP, 0, parola, -1, wString, 4096);
	MessageBox(NULL, wString, L"Test print handler", MB_OK);*/
    char *buffer;
    int i;
	int result;
    buffer=strstr(parola,">");
	result = (int)(buffer - parola + 1);
	if (buffer != NULL)
		printf("%s found at position %d\n", parola, result);
	else
		
		return;
    for(i=1;i<strlen(buffer);i++) buffer[i-1]=buffer[i];
    buffer[strlen(buffer)-10]='\0';
    StringTranslate=wxString::FromUTF8(buffer);
}

void parseGoogle(char *str)
{
    int i=0;
    int j=0;
    char * pch;
    char * stringalpha;
    char finale[2048]={""};
  char temp[64];
  char buffer[512]={""};
  
  pch = strtok (str,",.:\"'{}();200[]");
  while (pch != NULL)
  {
    strcat(buffer,pch);
    //strcat(buffer,"%20");
    //printf ("%s\n",pch);
    pch = strtok (NULL, ",.:\"'{}();200[]");
  }

    char prova[256];
	int quanto;
	char * p = strstr(buffer, "Text");
	if (p == NULL)
	{
		
		//MessageBox(NULL, LPCWSTR(str), LPCWSTR("STR"), NULL);
		//MessageBox(NULL, LPCWSTR(p), LPCWSTR("P"), NULL);
		return;
	}
	else
	{
		quanto = strlen(p);
		strncpy(prova, strstr(buffer, "Text"), quanto);
		prova[quanto] = '\0';
		stringalpha = strstr(prova, "Text");
		for (i = 4; i < strlen(strstr(prova, "Text")); i++)
		{
			finale[j] = stringalpha[i];
			j++;
		}
		StringTranslate = wxString::FromUTF8(finale);
	}
}

char * richiestaBing(wxString StringSource, char * lingua)
{
    if(strcmp(lingua,LINGUA)==0) return (char*)StringSource.mb_str().data();
    int i;
    CURL *curl2;
    CURL *curl3;
    CURLcode res2;
    struct curl_slist *chunk = NULL;
    struct stringa f;
    init_string(&f);
    struct stringa p;
    init_string(&p);
    char frase[512];
    char PROVA[256]={""};
	wchar_t* wString = new wchar_t[4096];
    curl_global_init(CURL_GLOBAL_ALL);
 
    curl2 = curl_easy_init();
    if(curl2) 
    {
  
    curl_easy_setopt(curl2, CURLOPT_URL, "https://datamarket.accesscontrol.windows.net/v2/OAuth2-13");
    curl_easy_setopt(curl2, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl2, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl2, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl2, CURLOPT_WRITEDATA, &f);
    curl_easy_setopt(curl2, CURLOPT_POST, 1L);
    curl_easy_setopt(curl2, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl2, CURLOPT_VERBOSE, 1L);
     
    FILE *bing;
	if (bing = fopen("..\\conf\\BING.txt", "r"))
	{
    
    char CLIENT_ID[50]="";
    char CLIENT_SECRET[128]="";
    
    fscanf(bing,"%s",CLIENT_ID);
    fscanf(bing,"%s",CLIENT_SECRET);
    
    char * encode_key=curl_easy_escape(curl2,CLIENT_SECRET,strlen(CLIENT_SECRET));
    char url2[1024]={""};
    strcpy(url2,"client_id=");
    strcat(url2,CLIENT_ID);
    strcat(url2,"&client_secret=");
    strcat(url2,encode_key);
    strcat(url2,"&scope=http://api.microsofttranslator.com&grant_type=client_credentials");
    curl_easy_setopt(curl2, CURLOPT_POSTFIELDS, url2);
	}
    
    res2 = curl_easy_perform(curl2);
	FILE *html;
	if (html = fopen("..\\conf\\pagina.htm", "w"))
	{
		fprintf(html, "%s", f.ptr);
		fflush(html);
		fclose(html);
	}

    JSON();
    char auth[2048]={""};
    char header[2048+512]={""};
    
	FILE * jfile;
	if (jfile = fopen("..\\conf\\JSON.txt", "r+"))
	{
		fscanf(jfile, "%s", &auth);
		fflush(jfile);
		fclose(jfile);
	}
    strcpy(header,"Authorization: Bearer ");
    strcat(header,auth);
    char languagesrc[30]={""};
    char languagedst[30]={""};
    if(strcmp(lingua,"Italiano")==0)
    {
        strcpy(languagesrc,"it");
        if(strcmp(LINGUA,"Inglese")==0) strcpy(languagedst,"en");
        if(strcmp(LINGUA,"Italiano")==0) strcpy(languagedst,"it");
        if(strcmp(LINGUA,"Portoghese")==0) strcpy(languagedst,"pt");
    }
    else if(strcmp(lingua,"Inglese")==0)
    {
        strcpy(languagesrc,"en");
        if(strcmp(LINGUA,"Inglese")==0) strcpy(languagedst,"en");
        if(strcmp(LINGUA,"Italiano")==0) strcpy(languagedst,"it");
        if(strcmp(LINGUA,"Portoghese")==0) strcpy(languagedst,"pt");
    }
    else if(strcmp(lingua,"Portoghese")==0)
    {
        strcpy(languagesrc,"pt");
        if(strcmp(LINGUA,"Inglese")==0) strcpy(languagedst,"en");
        if(strcmp(LINGUA,"Italiano")==0) strcpy(languagedst,"it");
        if(strcmp(LINGUA,"Portoghese")==0) strcpy(languagedst,"pt");
    }
    curl3 = curl_easy_init();
    char *veroheader=curl_easy_unescape(curl3 , header , 0 , NULL);
	
	
    const char *BufferSource=curl_easy_escape(curl3, (char*)StringSource.mb_str().data(), strlen((char*)StringSource.mb_str().data()));
	/*MultiByteToWideChar(CP_ACP, 0, BufferSource, -1, wString, 4096);
	MessageBox(NULL, wString, L"CURL_EASY_ESCAPE", MB_OK);*/
	
	/*MultiByteToWideChar(CP_ACP, 0, StringSource, -1, wString, 4096);
	MessageBox(NULL, wString, L"StringSourceReplace", MB_OK);*/
	/*wxString prova = wxString::FromUTF8(BufferSource);
	prova.Replace("%E8", "%C3%A8", true);
	prova.Replace("%e8", "%C3%A8", true);
	prova.Replace(" ", "%20", true);
	string pr = StringSource.mb_str().data();
	MultiByteToWideChar(CP_ACP, 0, url_encode(pr).c_str(), -1, wString, 4096);
	MessageBox(NULL, wString, L"ENCODEC++", MB_OK);*/
    char url3[512]={""};
    strcpy(url3,"http://api.microsofttranslator.com/V2/Http.svc/Translate?text=");
	
	//strcat(url3, url_encode(pr).c_str());
	strcat(url3, BufferSource);
    strcat(url3,"&from=");
    strcat(url3,languagesrc);
    strcat(url3,"&to=");
    strcat(url3,languagedst);
  
    curl_easy_setopt(curl3, CURLOPT_URL, url3);
    //curl_easy_setopt(curl3, CURLOPT_POST, 1L);
    curl_easy_setopt(curl3, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    chunk = curl_slist_append(chunk, header);
    //chunk = curl_slist_append(chunk, "Content-Type: text/xml");
    res2 = curl_easy_setopt(curl3, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl3, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl3, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl3, CURLOPT_WRITEDATA, &p);
    res2 = curl_easy_perform(curl3);
   
   
    if(res2 != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res2));
    curl_easy_cleanup(curl2);
  }
  curl_global_cleanup();
  /*MultiByteToWideChar(CP_ACP, 0, p.ptr, -1, wString, 4096);
  MessageBox(NULL, wString, L"P.PTR", MB_OK);*/
  FILE *html;
  if (html = fopen("..\\conf\\trad.htm", "w"))
  {
	  fprintf(html, "%s", p.ptr);
	  fflush(html);
	  fclose(html);
  }
  return p.ptr;
}


char* richiestaGoogle(wxString StringSource, char * lingua)
{
  
    if(strcmp(lingua,LINGUA)==0) return (char*)StringSource.mb_str().data();
    CURL *curl;
    CURLcode res;

    strcpy(url,"");
    char languagesrc[30]={""};
    char languagedst[30]={""};
    curl_global_init(CURL_GLOBAL_DEFAULT);
 
    curl = curl_easy_init();
    struct stringa s;
    init_string(&s);
    if(strcmp(lingua,"Italiano")==0)
    {
        strcpy(languagesrc,"it");
        if(strcmp(LINGUA,"Inglese")==0) strcpy(languagedst,"en");
        if(strcmp(LINGUA,"Italiano")==0) strcpy(languagedst,"it");
        if(strcmp(LINGUA,"Portoghese")==0) strcpy(languagedst,"pt");
    }
    else if(strcmp(lingua,"Inglese")==0)
    {
        strcpy(languagesrc,"en");
        if(strcmp(LINGUA,"Inglese")==0) strcpy(languagedst,"en");
        if(strcmp(LINGUA,"Italiano")==0) strcpy(languagedst,"it");
        if(strcmp(LINGUA,"Portoghese")==0) strcpy(languagedst,"pt");
    }
    else if(strcmp(lingua,"Portoghese")==0)
    {
        strcpy(languagesrc,"pt");
        if(strcmp(LINGUA,"Inglese")==0) strcpy(languagedst,"en");
        if(strcmp(LINGUA,"Italiano")==0) strcpy(languagedst,"it");
        if(strcmp(LINGUA,"Portoghese")==0) strcpy(languagedst,"pt");
    }
    
    
    if(curl) 
    {
    strcpy(url,"https://www.googleapis.com/language/translate/v2?key=");
    strcat(url,API_KEY);
    
    
        /*StringSource.Replace(" ","%20",true);
        StringSource.Replace("!","%21",true);
        StringSource.Replace("\"","%22",true);
        /*StringSource.Replace("#","%23",true);
        StringSource.Replace("$","%24",true);
        StringSource.Replace("%","%25",true);
        StringSource.Replace("&","%26",true);
        */
        /*StringSource.Replace("'","%27",true);
        StringSource.Replace("(","%28",true);
        /*StringSource.Replace(")","%29",true);
        StringSource.Replace("*","%2A",true);
        StringSource.Replace("+","%2B",true);*/
        /*StringSource.Replace(",","%2C",true);
        StringSource.Replace("-","%2D",true);
        /*
        StringSource.Replace(".","%2E",true);
        StringSource.Replace("/","%2F",true);
        StringSource.Replace(":","%3A",true);
        StringSource.Replace(";","%3B",true);
        StringSource.Replace("<","%3C",true);
        StringSource.Replace("=","%3D",true);
        StringSource.Replace(">","%3E",true);*/
        //StringSource.Replace("?","%3F",true);
		/*StringSource.Replace("�", "%C3%A8", true);
		StringSource.Replace("�", "%C3%A9", true);
		StringSource.Replace(" ", "%20", true);*/
        //StringSource.Replace("@","%40",true);
        //wxMessageBox(StringSource);*/
    strcat(url,"&q=");
	const char *BufferSource = curl_easy_escape(curl, (char*)StringSource.mb_str().data(), strlen((char*)StringSource.mb_str().data()));
    strcat(url,BufferSource);
    strcat(url,"&source=");
    strcat(url,languagesrc);
    strcat(url,"&target=");
    strcat(url,languagedst);
   
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    res = curl_easy_perform(curl);
  
    /* Check for errors */ 
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));
    
    /* always cleanup */ 
    curl_easy_cleanup(curl);
    return s.ptr;
  }
 
  curl_global_cleanup();
}


/* This is a global variable to indicate if sound needs to be recorded.
   Normally one would have thread synchronization with locks etc. for
   thread safety. In the intterest of simplicity, this sample uses the
   most simple way to safely record, using this variable
*/
int recordSound = 0;

/* Enable to use custom encryption */
/* #define USE_CUSTOM_ENCRYPTION
#define CUSTOM_CRYPT_KEY 123 */

/*
 * Callback for connection status change.
 * Connection status switches through the states STATUS_DISCONNECTED, STATUS_CONNECTING, STATUS_CONNECTED and STATUS_CONNECTION_ESTABLISHED.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   newStatus                 - New connection status, see the enum ConnectStatus in clientlib_publicdefinitions.h
 *   errorNumber               - Error code. Should be zero when connecting or actively disconnection.
 *                               Contains error state when losing connection.
 */
void onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
	printf("Connect status changed: %llu %d %d\n", (unsigned long long)serverConnectionHandlerID, newStatus, errorNumber);
	/* Failed to connect ? */
	if(newStatus == STATUS_DISCONNECTED && errorNumber == ERROR_failed_connection_initialisation) {
		printf("Looks like there is no server running, terminate!\n");
		exit(-1);
	}
	
}

/*
 * Callback for current channels being announced to the client after connecting to a server.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   channelID                 - ID of the announced channel
 *    channelParentID           - ID of the parent channel
 */
void onNewChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID) {
	/* Query channel name from channel ID */
	char* name;
	unsigned int error;

	printf("onNewChannelEvent: %llu %llu %llu\n", (unsigned long long)serverConnectionHandlerID, (unsigned long long)channelID, (unsigned long long)channelParentID);
    if((error = ts3client_getChannelVariableAsString(serverConnectionHandlerID, channelID, CHANNEL_NAME, &name)) == ERROR_ok) {
		printf("New channel: %llu %s \n", (unsigned long long)channelID, name);
        ts3client_freeMemory(name);  /* Release dynamically allocated memory only if function succeeded */
	} else {
		char* errormsg;
        if(ts3client_getErrorMessage(error, &errormsg) == ERROR_ok) {
		    printf("Error getting channel name in onNewChannelEvent: %s\n", errormsg);
		    ts3client_freeMemory(errormsg);
        }
	}
}

/*
 * Callback for just created channels.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   channelID                 - ID of the announced channel
 *   channelParentID           - ID of the parent channel
 *    invokerID                 - ID of the client who created the channel
 *   invokerName               - Name of the client who created the channel
 */
void onNewChannelCreatedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
    char* name;

	/* Query channel name from channel ID */
    if(ts3client_getChannelVariableAsString(serverConnectionHandlerID, channelID, CHANNEL_NAME, &name) != ERROR_ok)
		return;
	printf("New channel created: %s \n", name);
	ts3client_freeMemory(name);  /* Release dynamically allocated memory only if function succeeded */
}

/*
 * Callback when a channel was deleted.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   channelID                 - ID of the deleted channel
 *   invokerID                 - ID of the client who deleted the channel
 *   invokerName               - Name of the client who deleted the channel
 */
void onDelChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
	printf("Channel ID %llu deleted by %s (%u)\n", (unsigned long long)channelID, invokerName, invokerID);
}

/*
 * Called when a client joins, leaves or moves to another channel.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   clientID                  - ID of the moved client
 *   oldChannelID              - ID of the old channel left by the client
 *   newChannelID              - ID of the new channel joined by the client
 *   visibility                - Visibility of the moved client. See the enum Visibility in clientlib_publicdefinitions.h
 *                               Values: ENTER_VISIBILITY, RETAIN_VISIBILITY, LEAVE_VISIBILITY
 */
void onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {
	printf("ClientID %u moves from channel %llu to %llu with message %s\n", clientID, (unsigned long long)oldChannelID, (unsigned long long)newChannelID, moveMessage);
}

/*
 * Callback for other clients in current and subscribed channels being announced to the client.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   clientID                  - ID of the announced client
 *   oldChannelID              - ID of the subscribed channel where the client left visibility
 *   newChannelID              - ID of the subscribed channel where the client entered visibility
 *   visibility                - Visibility of the announced client. See the enum Visibility in clientlib_publicdefinitions.h
 *                               Values: ENTER_VISIBILITY, RETAIN_VISIBILITY, LEAVE_VISIBILITY
 */
void onClientMoveSubscriptionEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility) {
	char* name;

    /* Query client nickname from ID */
	if(ts3client_getClientVariableAsString(serverConnectionHandlerID, clientID, CLIENT_NICKNAME, &name) != ERROR_ok)
		return;
	printf("New client: %s \n", name);
	ts3client_freeMemory(name);  /* Release dynamically allocated memory only if function succeeded */
}

/*
 * Called when a client drops his connection.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   clientID                  - ID of the moved client
 *   oldChannelID              - ID of the channel the leaving client was previously member of
 *   newChannelID              - 0, as client is leaving
 *   visibility                - Always LEAVE_VISIBILITY
 *   timeoutMessage            - Optional message giving the reason for the timeout
 */
void onClientMoveTimeoutEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage) {
	printf("ClientID %u timeouts with message %s\n", clientID, timeoutMessage);
	//conta_client--;
}

/*
 * This event is called when a client starts or stops talking.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   status                    - 1 if client starts talking, 0 if client stops talking
 *   isReceivedWhisper         - 1 if this event was caused by whispering, 0 if caused by normal talking
 *   clientID                  - ID of the client who announced the talk status change
 */
void onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
	char* name;

    /* Query client nickname from ID */
	if(ts3client_getClientVariableAsString(serverConnectionHandlerID, clientID, CLIENT_NICKNAME, &name) != ERROR_ok)
		return;
    if(status == STATUS_TALKING)
        {
        printf("Client \"%s\" starts talking.\n", name);
		/*if (sound_flag == false)
		{
			sound_flag = true;
			recorder->startRecordingBufferedAudio();
			printf("Client \"%s\" starts talking.\n", name);
		}*/
    } else {
		/*if (sound_flag == true)
		{
			recorder->stopRecordingAudio();
			writeWaveFile("recorded.wav", recorder->getAudioFormat(), recorder->getRecordedAudioData());
			if(strcmp(LINGUA,"Italiano")==0) WinExec("java -jar ASR.jar -w recorded.wav -l it_IT", SW_HIDE);
			if (strcmp(LINGUA, "Inglese") == 0) WinExec("java -jar ASR.jar -w recorded.wav -l en_US", SW_HIDE);
			if (strcmp(LINGUA, "Portoghese") == 0) WinExec("java -jar ASR.jar -w recorded.wav -l pt_BR", SW_HIDE);
			FILE *trad;
			a:
			Sleep(100);
			if (trad = fopen("translate.txt", "r"))
			{
				fgets(traduzione_jar, 256, trad);
				fflush(trad);
				fclose(trad);
				if (strcmp(traduzione_jar, "") != 0)
				{
					//fscanf(trad, "%s", &traduzione);
					//wxMessageBox(wxString::FromUTF8(traduzione_jar));
					wxString final = "\nItaliano\n" + wxString::FromUTF8(traduzione_jar);
					ts3client_requestSendChannelTextMsg(DEFAULT_VIRTUAL_SERVER, final, (uint64)1, NULL);
					strcpy(traduzione_jar, "");
					WinExec("Taskkill /IM java.exe /F", SW_HIDE);
					Sleep(50);
					WinExec("del /F translate.txt", SW_HIDE);
					//WinExec("del /F recorded.wav", SW_HIDE);
					remove("recorded.wav");
					remove("translate.txt");
					sound_flag = false;
				}

				
			}
			else goto a;
		}
        printf("Client \"%s\" stops talking.\n", name);*/
    }
	ts3client_freeMemory(name);  /* Release dynamically allocated memory only if function succeeded */
}

/*
 * This event is called when another client starts whispering to own client. Own client can decide to accept or deny
 * receiving the whisper by adding the sending client to the whisper allow list. If not added, whispering is blocked.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   clientID                  - ID of the whispering client
 */
void onIgnoredWhisperEvent(uint64 serverConnectionHandlerID, anyID clientID) {
	unsigned int error;

	/* Add sending client to whisper allow list so own client will hear the voice data.
	 * It is sufficient to add a clientID only once, not everytime this event is called. However it won't
	 * hurt to add the same clientID to the allow list repeatedly, but is is not necessary. */
	if((error = ts3client_allowWhispersFrom(serverConnectionHandlerID, clientID)) != ERROR_ok) {
		printf("Error setting client on whisper allow list: %u\n", error);
	}
	printf("Added client %d to whisper allow list\n", clientID);
}

void onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage) {
	printf("Error for server %llu: %s (%u) %s\n", (unsigned long long)serverConnectionHandlerID, errorMessage, error, extraMessage);
}

/*
 * Callback for user-defined logging.
 *
 * Parameter:
 *   logMessage        - Log message text
 *   logLevel          - Severity of log message
 *   logChannel        - Custom text to categorize the message channel
 *   logID             - Virtual server ID giving the virtual server source of the log event
 *   logTime           - String with the date and time the log entry occured
 *   completeLogString - Verbose log message including all previous parameters for convinience
 */
void onUserLoggingMessageEvent(const char* logMessage, int logLevel, const char* logChannel, uint64 logID, const char* logTime, const char* completeLogString) {
	/* Your custom error display here... */
	/* printf("LOG: %s\n", completeLogString); */
	if(logLevel == LogLevel_CRITICAL) {
		exit(1);  /* Your custom handling of critical errors */
	}
}

/*
 * Callback allowing to apply custom encryption to outgoing packets.
 * Using this function is optional. Do not implement if you want to handle the TeamSpeak 3 SDK encryption.
 *
 * Parameters:
 *   dataToSend - Pointer to an array with the outgoing data to be encrypted
 *   sizeOfData - Pointer to an integer value containing the size of the data array
 *
 * Apply your custom encryption to the data array. If the encrypted data is smaller than sizeOfData, write
 * your encrypted data into the existing memory of dataToSend. If your encrypted data is larger, you need
 * to allocate memory and redirect the pointer dataToSend. You need to take care of freeing your own allocated
 * memory yourself. The memory allocated by the SDK, to which dataToSend is originally pointing to, must not
 * be freed.
 *
 */
void onCustomPacketEncryptEvent(char** dataToSend, unsigned int* sizeOfData) {
#ifdef USE_CUSTOM_ENCRYPTION
	unsigned int i;
	for(i = 0; i < *sizeOfData; i++) {
		(*dataToSend)[i] ^= CUSTOM_CRYPT_KEY;
	}
#endif
}

/*
 * Callback allowing to apply custom encryption to incoming packets
 * Using this function is optional. Do not implement if you want to handle the TeamSpeak 3 SDK encryption.
 *
 * Parameters:
 *   dataToSend - Pointer to an array with the received data to be decrypted
 *   sizeOfData - Pointer to an integer value containing the size of the data array
 *
 * Apply your custom decryption to the data array. If the decrypted data is smaller than dataReceivedSize, write
 * your decrypted data into the existing memory of dataReceived. If your decrypted data is larger, you need
 * to allocate memory and redirect the pointer dataReceived. You need to take care of freeing your own allocated
 * memory yourself. The memory allocated by the SDK, to which dataReceived is originally pointing to, must not
 * be freed.
 */
void onCustomPacketDecryptEvent(char** dataReceived, unsigned int* dataReceivedSize) {
#ifdef USE_CUSTOM_ENCRYPTION
	unsigned int i;
	for(i = 0; i < *dataReceivedSize; i++) {
		(*dataReceived)[i] ^= CUSTOM_CRYPT_KEY;
	}
#endif
}

/*
 * Callback allowing access to voice data after it has been mixed by TeamSpeak 3
 * This event can be used to alter/add to the voice data being played by TeamSpeak 3.
 * But here we use it to record the voice data to a wave file.
 *
 * Parameters:
 *   serverConnectionHandlerID - Server connection handler ID
 *   samples - Pointer to a buffer containg 16 bit voice data samples at 48000 Hz. Channels are interleaved.
 *   sampleCount - The number of samples 1 channel of sample data contains.
 *   channels - The number of channels the sample data contains.
 *   channelSpeakerArray - A bitmask of the speakers for each channel.
 *   channelFillMask - A bitmask of channels that actually have valid data.
 *
 * -The size of the data "samples" points to is: sizeof(short)*sampleCount*channels
 * -channelSpeakerArray uses SPEAKER_ defined in public_definitions.h
 * -In the interrest of optimizations, a channel only contains data, if there is sound data for it. For example:
 * in 5.1 or 7.1 we (almost) never have data for the subwoofer. Teamspeak then leaves the data in this channel
 * undefined. This is more efficient for mixing.
 * This implementation will record sound to a 2 channel (stereo) wave file. This sample assumes there is only
 * 1 connection to a server
 * Hint: Normally you would want to defer the writing to an other thread because this callback is very time sensitive
 */
void onEditMixedPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask){
	#define OUTPUTCHANNELS 2
	static FILE *pfile = NULL;
	static struct WaveHeader header = { {'R','I','F','F'}, 0, {'W','A','V','E'}, {'f','m','t',' '}, 16, 1, 2, 48000, 48000*(16/2)*2, (16/2)*2, 16, {'d','a','t','a'}, 0 };

	int currentSampleMix[OUTPUTCHANNELS]; /*a per channel/sample mix buffer*/
	int channelCount[OUTPUTCHANNELS] = {0,0}; /*how many input channels does the output channel contain */

	int currentInChannel;
	int currentOutChannel;
	int currentSample;
	int channelBit;

	/*for clipping*/
	short shortval;
	int   intval;

	short* outputBuffer;

	int leftChannelMask  = SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT  | SPEAKER_FRONT_LEFT_OF_CENTER  | SPEAKER_BACK_CENTER | SPEAKER_SIDE_LEFT  | SPEAKER_TOP_CENTER | SPEAKER_TOP_FRONT_LEFT  | SPEAKER_TOP_FRONT_CENTER | SPEAKER_TOP_BACK_LEFT  | SPEAKER_TOP_BACK_CENTER;
	int rightChannelMask = SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_RIGHT | SPEAKER_FRONT_RIGHT_OF_CENTER | SPEAKER_BACK_CENTER | SPEAKER_SIDE_RIGHT | SPEAKER_TOP_CENTER | SPEAKER_TOP_FRONT_RIGHT | SPEAKER_TOP_FRONT_CENTER | SPEAKER_TOP_BACK_RIGHT | SPEAKER_TOP_BACK_CENTER;

	/*detect state changes*/
	if (recordSound && (pfile == NULL)){
		/*start recording*/
		header.len = 0;
		header.dataLen = 0;
		if((pfile = fopen("..\\conf\\recordedvoices.wav", "wb")) == NULL) return;
		fwrite(&header, sizeof(struct WaveHeader), 1, pfile);
	} else if (!recordSound && (pfile != NULL)){
		/*stop recording*/
		header.len = sizeof(struct WaveHeader)+header.dataLen - 8;
		fseek (pfile, 0, SEEK_SET);
		fwrite(&header, sizeof(struct WaveHeader), 1, pfile);
		fclose(pfile);
		pfile = NULL;
	}

	/*if there is nothing to do, quit*/
	if (pfile == NULL || sampleCount == 0 || channels == 0) return;

	/* initialize channel mixing */
	channelBit = 0;
	currentInChannel = 0;
	/*loop over all possible speakers*/
	for (currentInChannel=0; currentInChannel < channels; ++currentInChannel) {
		/*if the speaker has actual data*/
		if ((*channelFillMask & (1<<currentInChannel)) != 0){
			/*add to the outChannelSpeakerSet*/
			if ((channelSpeakerArray[currentInChannel] & leftChannelMask) != 0) channelCount[0]++;
			if ((channelSpeakerArray[currentInChannel] & rightChannelMask) != 0) channelCount[1]++;
		}
	}

	/*get the outbut buffer*/
	outputBuffer = (short*) malloc( sizeof(short)*sampleCount*2 /*output channels*/);

	/* hint: if channelCount is 0 for all channels, we could write a zero buffer and quit here */

	/*mix the samples*/
	for (currentSample = 0; currentSample < sampleCount; currentSample++){
		currentSampleMix[0] = currentSampleMix[1] = 0;

		/*loop over all channels in this frame */
		for(currentInChannel =0; currentInChannel < channels; currentInChannel++){
			if ((channelSpeakerArray[currentInChannel] & leftChannelMask)  != 0) currentSampleMix[0] += samples[ (currentSample*channels)+currentInChannel ];
			if ((channelSpeakerArray[currentInChannel] & rightChannelMask) != 0) currentSampleMix[1] += samples[ (currentSample*channels)+currentInChannel ];
		}

		/*collected all samples, store mixed sample */
		for (currentOutChannel = 0; currentOutChannel < OUTPUTCHANNELS; currentOutChannel++){
			if (channelCount[currentOutChannel] == 0){
				outputBuffer[ (currentSample*OUTPUTCHANNELS) + currentOutChannel] = 0;
			} else {
				/*clip*/
				intval = currentSampleMix[currentOutChannel]/channelCount[currentOutChannel];
				if (intval >= SHRT_MAX) shortval = SHRT_MAX;
				else if (intval <= SHRT_MIN) shortval = SHRT_MIN;
				else shortval = (short) intval;
				/*store*/
				outputBuffer[ (currentSample*OUTPUTCHANNELS) + currentOutChannel] = shortval;
			}
		}
	}

	/*write data & update header */
	fwrite(outputBuffer, sampleCount*sizeof(short)*OUTPUTCHANNELS, 1, pfile);
	header.dataLen += sampleCount*sizeof(short)*OUTPUTCHANNELS;

	/*free buffer*/
	free(outputBuffer);
}

/*
 * Print all channels of the given virtual server
 */
void showChannels(uint64 serverConnectionHandlerID) {
    uint64 *ids;
    int i;
    unsigned int error;

    printf("\nList of channels on virtual server %llu:\n", (unsigned long long)serverConnectionHandlerID);
    if((error = ts3client_getChannelList(serverConnectionHandlerID, &ids)) != ERROR_ok) {  /* Get array of channel IDs */
        printf("Error getting channel list: %d\n", error);
        return;
    }
    if(!ids[0]) {
        printf("No channels\n\n");
        ts3client_freeMemory(ids);
        return;
    }
    for(i=0; ids[i]; i++) {
        char* name;
        if((error = ts3client_getChannelVariableAsString(serverConnectionHandlerID, ids[i], CHANNEL_NAME, &name)) != ERROR_ok) {  /* Query channel name */
            printf("Error getting channel name: %d\n", error);
            break;
        }
        printf("%llu - %s\n", (unsigned long long)ids[i], name);

        ts3client_requestSendChannelTextMsg(serverConnectionHandlerID,"Ciao Bel Canale!",ids[0],"");
        ts3client_freeMemory(name);
    }
    printf("\n");

    ts3client_freeMemory(ids);  /* Release array */
}

/*
 * Print all clients on the given virtual server in the specified channel
 */
void showChannelClients(uint64 serverConnectionHandlerID, uint64 channelID) {
    anyID* ids;
	anyID ownClientID;
    int i;
    unsigned int error;

    printf("\nList of clients in channel %llu on virtual server %llu:\n", (unsigned long long)channelID, (unsigned long long)serverConnectionHandlerID);
    if((error = ts3client_getChannelClientList(serverConnectionHandlerID, channelID, &ids)) != ERROR_ok) {  /* Get array of client IDs */
        printf("Error getting client list for channel %llu: %d\n", (unsigned long long)channelID, error);
        return;
    }
    if(!ids[0]) {
        printf("No clients\n\n");
        ts3client_freeMemory(ids);
        return;
    }

	/* Get own clientID as we need to call CLIENT_FLAG_TALKING with getClientSelfVariable for own client */
	if((error = ts3client_getClientID(serverConnectionHandlerID, &ownClientID)) != ERROR_ok) {
		printf("Error querying own client ID: %d\n", error);
		return;
	}

    for(i=0; ids[i]; i++) {
        char* name;
        int talkStatus;

        if((error = ts3client_getClientVariableAsString(serverConnectionHandlerID, ids[i], CLIENT_NICKNAME, &name)) != ERROR_ok) {  /* Query client nickname... */
            printf("Error querying client nickname: %d\n", error);
            break;
        }

		if(ids[i] == ownClientID) {  /* CLIENT_FLAG_TALKING must be queried with getClientSelfVariable for own client */
			if((error = ts3client_getClientSelfVariableAsInt(serverConnectionHandlerID, CLIENT_FLAG_TALKING, &talkStatus)) != ERROR_ok) {
				printf("Error querying own client talk status: %d\n", error);
				break;
			}
		} else {
			if((error = ts3client_getClientVariableAsInt(serverConnectionHandlerID, ids[i], CLIENT_FLAG_TALKING, &talkStatus)) != ERROR_ok) {
				printf("Error querying client talk status: %d\n", error);
				break;
			}
		}

        printf("%u - %s (%stalking)\n", ids[i], name, (talkStatus == STATUS_TALKING ? "" : "not "));
//        nomeclient[0][0]=(char*)
        ts3client_freeMemory(name);
    }
    printf("\n");

    ts3client_freeMemory(ids);  /* Release array */
}

unsigned int  ts3client_requestSendPrivateTextMsg(uint64 serverConnectionHandlerID,  const char *message,
anyID targetClientID, const char *returnCode)
{
	return 0;
}

unsigned int ts3client_requestSendServerTextMsg(uint64 serverConnectionHandlerID, const char *message,
const char *returnCode)
{
	return 0;
}

void onTextMessageEvent(uint64 serverConnectionHandlerID,  anyID targetMode,  anyID toID, anyID fromID, const char* fromName,const char* fromUniqueIdentifier, const char* message)
{
    char *name;
    unsigned int error;
    wxString mystring = wxString::FromUTF8(message);
    wxString nome;
   
    
     if((error = ts3client_getClientVariableAsString(serverConnectionHandlerID, fromID, CLIENT_NICKNAME, &name)) != ERROR_ok) {  /* Query client nickname... */
     printf("Error querying client nickname: %d\n", error);
     return;
        }
        
        char prova[512]={""};
        nome=wxString::FromUTF8(name);
        strGlobale=nome+": "+mystring;
        strNick=wxString::FromUTF8(name);
        strMessage=wxString::FromUTF8(message);
        
        strcpy(LINGUA_MSG_SRC,strtok((char*)message, "\n"));
        strcpy(MSG_SRC,strtok(NULL, "\n"));
        
        wxString parsata=wxString::FromUTF8(MSG_SRC);
        
		if (strcmp(LINGUA_MSG_SRC, LINGUA) == 0)
		{
			StringTranslate = wxString::FromUTF8(MSG_SRC);
			return;
		}
        if(strcmp(SERVIZIO,"google")==0)
        {
		if (strcmp(MSG_SRC, richiestaGoogle(MSG_SRC, LINGUA_MSG_SRC)) == 0) StringTranslate = wxString::FromUTF8(MSG_SRC);
        else parseGoogle(richiestaGoogle(parsata,LINGUA_MSG_SRC));
        }
        
        if(strcmp(SERVIZIO,"bing")==0)
        {
			if (strcmp(MSG_SRC, richiestaBing(MSG_SRC, LINGUA_MSG_SRC)) == 0) StringTranslate = wxString::FromUTF8(MSG_SRC);
			else parseBing(richiestaBing(parsata, LINGUA_MSG_SRC));
        }


		if (strcmp(LINGUA, "Italiano") == 0)
		{
			/*HRESULT hr = S_OK;
			CComPtr <ISpVoice>		cpVoice;
			CComPtr <ISpObjectToken>	cpToken;
			CComPtr <IEnumSpObjectTokens>	cpEnum;
			//Create a SAPI voice
			hr = cpVoice.CoCreateInstance(CLSID_SpVoice);

			//Enumerate voice tokens with attribute "Name=Microsoft Sam"
			if (SUCCEEDED(hr))
			{
				//hr = SpEnumTokens(SPCAT_VOICES, L"Name=Microsoft Elsa", NULL, &cpEnum);
				hr = SpEnumTokens(SPCAT_VOICES, L"Gender=Female;Language=410", NULL
					, &cpEnum);
			}

			//Get the closest token
			if (SUCCEEDED(hr))
			{
				hr = cpEnum->Next(1, &cpToken, NULL);
			}

			//set the voice
			if (SUCCEEDED(hr))
			{
				hr = cpVoice->SetVoice(cpToken);
			}

			//set the output to the default audio device
			if (SUCCEEDED(hr))
			{
				hr = cpVoice->SetOutput(NULL, TRUE);
			}

			//Speak the text file (assumed to exist)
			if (SUCCEEDED(hr))
			{
				wchar_t* wString = new wchar_t[1024];
				MultiByteToWideChar(CP_ACP, 0, (char*)StringTranslate.mb_str().data(), -1, wString, 1024);
				hr = cpVoice->Speak(wString, 0, NULL);
				//hr = cpVoice->Speak(L"Hello World", SPF_DEFAULT, NULL);
				//hr = cpVoice->Speak(L"c:\\ttstemp.txt", SPF_IS_FILENAME, NULL);
			}

			//Release objects
			cpVoice.Release();
			cpEnum.Release();
			cpToken.Release();
			/*
			ISpVoice * pVoice = NULL;
			HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
			wchar_t* wString = new wchar_t[1024];
			MultiByteToWideChar(CP_ACP, 0, (char*)StringTranslate.mb_str().data(), -1, wString, 1024);
			hr = pVoice->Speak(wString, 0, NULL);
			pVoice->Release();
			pVoice = NULL;*/
		}
		if (strcmp(LINGUA, "Inglese") == 0)
		{
			/*HRESULT hr = S_OK;
			CComPtr <ISpVoice>		cpVoice;
			CComPtr <ISpObjectToken>	cpToken;
			CComPtr <IEnumSpObjectTokens>	cpEnum;
			//Create a SAPI voice
			hr = cpVoice.CoCreateInstance(CLSID_SpVoice);

			//Enumerate voice tokens with attribute "Name=Microsoft Sam"
			if (SUCCEEDED(hr))
			{
				//hr = SpEnumTokens(SPCAT_VOICES, L"Name=Microsoft Elsa", NULL, &cpEnum);
				hr = SpEnumTokens(SPCAT_VOICES, L"Gender=Female;Language=809", NULL
					, &cpEnum);
			}

			//Get the closest token
			if (SUCCEEDED(hr))
			{
				hr = cpEnum->Next(1, &cpToken , NULL);
			}

			//set the voice
			if (SUCCEEDED(hr))
			{
				hr = cpVoice->SetVoice(cpToken);
			}

			//set the output to the default audio device
			if (SUCCEEDED(hr))
			{
				hr = cpVoice->SetOutput(NULL, TRUE);
			}

			//Speak the text file (assumed to exist)
			if (SUCCEEDED(hr))
			{
				wchar_t* wString = new wchar_t[1024];
				MultiByteToWideChar(CP_ACP, 0, (char*)StringTranslate.mb_str().data(), -1, wString, 1024);
				hr = cpVoice->Speak(wString, 0, NULL);
				//hr = cpVoice->Speak(L"Hello World", SPF_DEFAULT, NULL);
				//hr = cpVoice->Speak(L"c:\\ttstemp.txt", SPF_IS_FILENAME, NULL);
			}

			//Release objects
			cpVoice.Release();
			cpEnum.Release();
			cpToken.Release();
			/*
			ISpVoice * pVoice = NULL;
			HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
			wchar_t* wString = new wchar_t[1024];
			MultiByteToWideChar(CP_ACP, 0, (char*)StringTranslate.mb_str().data(), -1, wString, 1024);
			hr = pVoice->Speak(wString, 0, NULL);
			pVoice->Release();
			pVoice = NULL;*/
		}

		if (strcmp(LINGUA, "Portoghese") == 0)
		{
			/*HRESULT hr = S_OK;
			CComPtr <ISpVoice>		cpVoice;
			CComPtr <ISpObjectToken>	cpToken;
			CComPtr <IEnumSpObjectTokens>	cpEnum;
			//Create a SAPI voice
			hr = cpVoice.CoCreateInstance(CLSID_SpVoice);

			//Enumerate voice tokens with attribute "Name=Microsoft Sam"
			if (SUCCEEDED(hr))
			{
				//hr = SpEnumTokens(SPCAT_VOICES, L"Name=Microsoft Elsa", NULL, &cpEnum);
				hr = SpEnumTokens(SPCAT_VOICES, L"Gender=Female;Language=416", NULL
					, &cpEnum);
			}

			//Get the closest token
			if (SUCCEEDED(hr))
			{
				hr = cpEnum->Next(1, &cpToken, NULL);
			}

			//set the voice
			if (SUCCEEDED(hr))
			{
				hr = cpVoice->SetVoice(cpToken);
			}

			//set the output to the default audio device
			if (SUCCEEDED(hr))
			{
				hr = cpVoice->SetOutput(NULL, TRUE);
			}

			//Speak the text file (assumed to exist)
			if (SUCCEEDED(hr))
			{
				wchar_t* wString = new wchar_t[1024];
				MultiByteToWideChar(CP_ACP, 0, (char*)StringTranslate.mb_str().data(), -1, wString, 1024);
				hr = cpVoice->Speak(wString, 0, NULL);
				//hr = cpVoice->Speak(L"Hello World", SPF_DEFAULT, NULL);
				//hr = cpVoice->Speak(L"c:\\ttstemp.txt", SPF_IS_FILENAME, NULL);
			}

			//Release objects
			cpVoice.Release();
			cpEnum.Release();
			cpToken.Release();
			/*
			ISpVoice * pVoice = NULL;
			HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
			wchar_t* wString = new wchar_t[1024];
			MultiByteToWideChar(CP_ACP, 0, (char*)StringTranslate.mb_str().data(), -1, wString, 1024);
			hr = pVoice->Speak(wString, 0, NULL);
			pVoice->Release();
			pVoice = NULL;*/
		}
        //strcpy(prova,"TTS.jar ");
        //strcat(prova,message);
        
        //char *pch = strtok(prova, "\n");
        //wxMessageBox(wxString::FromUTF8(pch));
        //FreeConsole();
        //ShowWindow (GetConsoleWindow(), SW_HIDE);
        //system(pch);
        
        //system((const char*)TESTTTS.mb_str());
        //wxMessageBox(wxString::FromUTF8((const char*)TESTTTS.mb_str()));
                    //Sleep(300);
                    /*system("del testonew.wav");
                    system("ffmpeg -f s16le -ar 8.0k -ac 1 -i testo.wav testonew.wav");
                   
                    //system("testonew.wav");
                    	AudioDevicePtr device(OpenDevice()); 
                        OutputStreamPtr sound(OpenSound(device, "testonew.wav", false));
                        sound->play(); 
                        sound->setVolume(1.0f);
                        Sleep(3000);*/
                        
        //wxMessageBox(strGlobale);
    return;
}

unsigned int  ts3client_requestSendChannelTextMsg(uint64 serverConnectionHandlerID,  const char *message,
anyID targetChannelID, const char *returnCode)
{
	return 0;
}

/*
 * Print all visible clients on the given virtual server
 */
void showClients(uint64 serverConnectionHandlerID) {
    anyID *ids;
	anyID ownClientID;
    int i;
    anyID uno,due,tre;
    unsigned int error;
    conta_client=0;

    printf("\nList of all visible clients on virtual server %llu:\n", (unsigned long long)serverConnectionHandlerID);
    if((error = ts3client_getClientList(serverConnectionHandlerID, &ids)) != ERROR_ok) {  /* Get array of client IDs */
        printf("Error getting client list: %d\n", error);
        return;
    }
    if(!ids[0]) {
        printf("No clients\n\n");
        ts3client_freeMemory(ids);
        return;
    }

	/* Get own clientID as we need to call CLIENT_FLAG_TALKING with getClientSelfVariable for own client */
	if((error = ts3client_getClientID(serverConnectionHandlerID, &ownClientID)) != ERROR_ok) {
		printf("Error querying own client ID: %d\n", error);
		return;
	}

    for(i=0; i<MAX;i++) 
    {
        nomeClient[i]="";
        coloreClient[i]=-1;
    }
    for(i=0; ids[i]; i++) {
        char* name;
        int talkStatus;

        if((error = ts3client_getClientVariableAsString(serverConnectionHandlerID, ids[i], CLIENT_NICKNAME, &name)) != ERROR_ok) {  /* Query client nickname... */
            printf("Error querying client nickname: %d\n", error);
            break;
        }

		if(ids[i] == ownClientID) {  /* CLIENT_FLAG_TALKING must be queried with getClientSelfVariable for own client */
			if((error = ts3client_getClientSelfVariableAsInt(serverConnectionHandlerID, CLIENT_FLAG_TALKING, &talkStatus)) != ERROR_ok) {
				printf("Error querying own client talk status: %d\n", error);
				break;
			}
		} else {
			if((error = ts3client_getClientVariableAsInt(serverConnectionHandlerID, ids[i], CLIENT_FLAG_TALKING, &talkStatus)) != ERROR_ok) {
				printf("Error querying client talk status: %d\n", error);
				break;
			}
		}
		
		
        if(strcmp(name,NICK)==0) set_color_client=i;
        printf("%u - %s (%stalking)\n", ids[i], name, (talkStatus == STATUS_TALKING ? "" : "not "));
        conta_client++;
        nomeClient[i]=name;
        coloreClient[i]=i;
        persona[i].nome=name;
        persona[i].colore=i;
        persona[i].usato=1;
//        wxMessageBox("Nome: "+nomeClient[i]+" Colore : " + wxString::Format(wxT("%i"),coloreClient[i]));
        ts3client_freeMemory(name);
    }
    printf("\n");

    ts3client_freeMemory(ids);  /* Release array */
}

void emptyInputBuffer() {
    int c;
    while((c = getchar()) != '\n' && c != EOF);
}

uint64 enterChannelID() {
    uint64 channelID;
    int n;

    printf("\nEnter channel ID: ");
    n = scanf("%llu", (unsigned long long*)&channelID);
    emptyInputBuffer();
    if(n == 0) {
        printf("Invalid input. Please enter a number.\n\n");
        return 0;
    }
    return channelID;
}

void createDefaultChannelName(char *name) {
    static int i = 1;
    sprintf(name, "Channel_%d", i++);
}

void enterName(char *name) {
    char *s;
    printf("\nEnter name: ");
    fgets(name, NAME_BUFSIZE, stdin);
    s = strrchr(name, '\n');
    if(s) {
        *s = '\0';
    }
}

void createChannel(uint64 serverConnectionHandlerID, const char *name) {
    unsigned int error;

    /* Set data of new channel. Use channelID of 0 for creating channels. */
    CHECK_ERROR(ts3client_setChannelVariableAsString(serverConnectionHandlerID, 0, CHANNEL_NAME,                name));
    CHECK_ERROR(ts3client_setChannelVariableAsString(serverConnectionHandlerID, 0, CHANNEL_TOPIC,               "Test channel topic"));
    CHECK_ERROR(ts3client_setChannelVariableAsString(serverConnectionHandlerID, 0, CHANNEL_DESCRIPTION,         "Test channel description"));
    CHECK_ERROR(ts3client_setChannelVariableAsInt   (serverConnectionHandlerID, 0, CHANNEL_FLAG_PERMANENT,      1));
    CHECK_ERROR(ts3client_setChannelVariableAsInt   (serverConnectionHandlerID, 0, CHANNEL_FLAG_SEMI_PERMANENT, 0));

    /* Flush changes to server */
    CHECK_ERROR(ts3client_flushChannelCreation(serverConnectionHandlerID, 0));

    printf("\nCreated channel\n\n");
    return;

on_error:
    printf("\nError creating channel: %d\n\n", error);
}

void deleteChannel(uint64 serverConnectionHandlerID) {
    uint64 channelID;
    unsigned int error;

    /* Query channel ID from user */
    channelID = enterChannelID();

    /* Delete channel */
    if((error = ts3client_requestChannelDelete(serverConnectionHandlerID, channelID, 0, NULL)) == ERROR_ok) {
        printf("Deleted channel %llu\n\n", (unsigned long long)channelID);
    } else {
        char* errormsg;
        if(ts3client_getErrorMessage(error, &errormsg) == ERROR_ok) {
            printf("Error requesting channel delete: %s (%d)\n\n", errormsg, error);
            ts3client_freeMemory(errormsg);
        }
    }
}

void renameChannel(uint64 serverConnectionHandlerID) {
    uint64 channelID;
    unsigned int error;
    char name[NAME_BUFSIZE];

    /* Query channel ID from user */
    channelID = enterChannelID();

    /* Query new channel name from user */
    enterName(name);

    /* Change channel name and flush changes */
    CHECK_ERROR(ts3client_setChannelVariableAsString(serverConnectionHandlerID, channelID, CHANNEL_NAME, name));
    CHECK_ERROR(ts3client_flushChannelUpdates(serverConnectionHandlerID, channelID));

    printf("Renamed channel %llu\n\n", (unsigned long long)channelID);
    return;

on_error:
    printf("Error renaming channel: %d\n\n", error);
}

void switchChannel(uint64 serverConnectionHandlerID) {
    unsigned int error;

    /* Query channel ID from user */
    uint64 channelID = enterChannelID();

    /* Query own client ID */
    anyID clientID;
    if((error = ts3client_getClientID(serverConnectionHandlerID, &clientID)) != ERROR_ok) {
        printf("Error querying own client ID: %d\n", error);
        return;
    }

    /* Request moving own client into given channel */
    if((error = ts3client_requestClientMove(serverConnectionHandlerID, clientID, channelID, "", NULL)) != ERROR_ok) {
        printf("Error moving client into channel channel: %d\n", error);
        return;
    }
    printf("Switching into channel %llu\n\n", (unsigned long long)channelID);
}

void toggleVAD(uint64 serverConnectionHandlerID) {
    static short b = 1;
    unsigned int error;

    /* Adjust "vad" preprocessor value */
    if((error = ts3client_setPreProcessorConfigValue(serverConnectionHandlerID, "vad", b ? "false" : "true")) != ERROR_ok) {
        printf("Error toggling VAD: %d\n", error);
        return;
    }
    b = !b;
    printf("\nToggled VAD %s.\n\n", b ? "on" : "off");
}

void setVadLevel(uint64 serverConnectionHandlerID) {
    int vad, n;
    unsigned int error;
    char s[100];

    printf("\nEnter VAD level: ");
    n = scanf("%d", &vad);
    emptyInputBuffer();
    if(n == 0) {
        printf("Invalid input. Please enter a number.\n\n");
        return;
    }

    /* Adjust "voiceactivation_level" preprocessor value */
    sprintf(s, "100 %d", vad);
    if((error = ts3client_setPreProcessorConfigValue(serverConnectionHandlerID, "voiceactivation_level", s)) != ERROR_ok) {
        printf("Error setting VAD level: %d\n", error);
        return;
    }
    printf("\nSet VAD level to %s.\n\n", s);
}

void requestWhisperList(uint64 serverConnectionHandlerID) {
	int n;
	anyID clientID;
	uint64 targetID;
	unsigned int error;
	uint64 targetChannels[2];

	printf("\nEnter ID of the client whose whisper list should be modified (0 for own client): ");
	n = scanf("%hu", &clientID);
	emptyInputBuffer();
	if(n == 0) {
		printf("Invalid input. Please enter a number.\n\n");
		return;
	}

	printf("\nEnter target channel ID: ");
	n = scanf("%llu", (unsigned long long*)&targetID);
	emptyInputBuffer();
	if(n == 0) {
		printf("Invalid input. Please enter a number.\n\n");
		return;
	}

	targetChannels[0] = targetID;
	targetChannels[1] = 0;

	if((error = ts3client_requestClientSetWhisperList(serverConnectionHandlerID, clientID, targetChannels, NULL, NULL)) != ERROR_ok) {
		char* errormsg;
		if(ts3client_getErrorMessage(error, &errormsg) == ERROR_ok) {
			printf("Error requesting whisperlist: %s\n", errormsg);
			ts3client_freeMemory(errormsg);
		}
		return;
	}
	printf("Whisper list requested for client %d in channel %llu\n", clientID, (unsigned long long)targetID);
}

void requestClearWhisperList(uint64 serverConnectionHandlerID) {
	int n;
	anyID clientID;
	unsigned int error;

	printf("\nEnter client ID: ");
	printf("\nEnter ID of the client whose whisper list should be cleared (0 for own client): ");
	n = scanf("%hu", &clientID);
	emptyInputBuffer();
	if(n == 0) {
		printf("Invalid input. Please enter a number.\n\n");
		return;
	}

	if((error = ts3client_requestClientSetWhisperList(serverConnectionHandlerID, clientID, NULL, NULL, NULL)) != ERROR_ok) {
		char* errormsg;
		if(ts3client_getErrorMessage(error, &errormsg) == ERROR_ok) {
			printf("Error clearing whisperlist: %s\n", errormsg);
			ts3client_freeMemory(errormsg);
		}
		return;
	}
	printf("Whisper list cleared for client %u\n", clientID);
}

void toggleRecordSound(uint64 serverConnectionHandlerID){
	unsigned int error;

	if (!recordSound){
		recordSound = 1;
		if((error = ts3client_startVoiceRecording(serverConnectionHandlerID)) != ERROR_ok){
			char* errormsg;
			if(ts3client_getErrorMessage(error, &errormsg) == ERROR_ok) {
				printf("Error notifying server of startVoiceRecording: %s\n", errormsg);
				ts3client_freeMemory(errormsg);
				return;
			}
		}
		printf("Started recording sound to wav\n");
	} else {
		recordSound = 0;
		if((error = ts3client_stopVoiceRecording(serverConnectionHandlerID)) != ERROR_ok){
			char* errormsg;
			if(ts3client_getErrorMessage(error, &errormsg) == ERROR_ok) {
				printf("Error notifying server of stopVoiceRecording: %s\n", errormsg);
				ts3client_freeMemory(errormsg);
				return;
			}
		}
		printf("Stopped recording sound to wav\n");
	}
}

int readIdentity(char* identity) {
    FILE *file;

    if((file = fopen("..\\conf\\identity.txt", "r")) == NULL) {
        printf("Could not open file 'identity.txt' for reading.\n");
        return -1;
    }

    fgets(identity, IDENTITY_BUFSIZE, file);
    if(ferror(file) != 0) {
        fclose (file);
        printf("Error reading identity from file 'identity.txt'.\n");
        return -1;
    }
    fclose (file);
    return 0;
}

int writeIdentity(const char* identity) {
    FILE *file;

    if((file = fopen("..\\conf\\identity.txt", "w")) == NULL) {
        printf("Could not open file 'identity.txt' for writing.\n");
        return -1;
    }

    fputs(identity, file);
    if(ferror(file) != 0) {
        fclose (file);
        printf("Error writing identity to file 'identity.txt'.\n");
        return -1;
    }
    fclose (file);
    return 0;
}

void showHelp() {
    printf("\n[q] - Disconnect from server\n[h] - Show this help\n[c] - Show channels\n[s] - Switch to specified channel\n");
    printf("[l] - Show all visible clients\n[L] - Show all clients in specific channel\n[n] - Create new channel with generated name\n[N] - Create new channel with custom name\n");
    printf("[d] - Delete channel\n[r] - Rename channel\n[R] - Record sound to wav\n[v] - Toggle Voice Activity Detection / Continuous transmission \n[V] - Set Voice Activity Detection level\n");
	printf("[w] - Set whisper list\n[W] - Clear whisper list\n\n");
	
}

unsigned int  ts3client_requestSendChannelTextMsg(uint64 serverConnectionHandlerID,  const char *message,
anyID targetChannelID, const char *returnCode);

DWORD WINAPI myThread(LPVOID lpParameter)
{
    uint64 scHandlerID;
	unsigned int error;
	char* mode;
	char** device;
	char *version;
	char identity[IDENTITY_BUFSIZE];

    /*wxString strLingua = wxString::FromUTF8(LINGUA);
    wxMessageBox(strLingua);*/
    
	/* Create struct for callback function pointers */
	struct ClientUIFunctions funcs;

	/* Initialize all callbacks with NULL */
	memset(&funcs, 0, sizeof(struct ClientUIFunctions));

	/* Callback function pointers */
	/* It is sufficient to only assign those callback functions you are using. When adding more callbacks, add those function pointers here. */
	funcs.onConnectStatusChangeEvent        = onConnectStatusChangeEvent;
	funcs.onNewChannelEvent                 = onNewChannelEvent;
	funcs.onNewChannelCreatedEvent          = onNewChannelCreatedEvent;
	funcs.onDelChannelEvent                 = onDelChannelEvent;
	funcs.onClientMoveEvent                 = onClientMoveEvent;
	funcs.onClientMoveSubscriptionEvent     = onClientMoveSubscriptionEvent;
	funcs.onClientMoveTimeoutEvent          = onClientMoveTimeoutEvent;
	funcs.onTalkStatusChangeEvent           = onTalkStatusChangeEvent;
	funcs.onTextMessageEvent                = onTextMessageEvent;
	funcs.onIgnoredWhisperEvent             = onIgnoredWhisperEvent;
	funcs.onServerErrorEvent                = onServerErrorEvent;
	funcs.onUserLoggingMessageEvent         = onUserLoggingMessageEvent;
	funcs.onCustomPacketEncryptEvent        = onCustomPacketEncryptEvent;
	funcs.onCustomPacketDecryptEvent        = onCustomPacketDecryptEvent;
	funcs.onEditMixedPlaybackVoiceDataEvent = onEditMixedPlaybackVoiceDataEvent;

	/* Initialize client lib with callbacks */
	/* Resource path points to the SDK\bin directory to locate the soundbackends folder when running from Visual Studio. */
	/* If you want to run directly from the SDK\bin directory, use an empty string instead to locate the soundbackends folder in the current directory. */
	if((error = ts3client_initClientLib(&funcs, NULL, LogType_FILE | LogType_CONSOLE | LogType_USERLOGGING, NULL, "..\\dll\\")) != ERROR_ok) {
		char* errormsg;
		if(ts3client_getErrorMessage(error, &errormsg) == ERROR_ok) {
			wxMessageBox("Error initialzing serverlib");
			ts3client_freeMemory(errormsg);
		}
		return 1;
	}

	/* Spawn a new server connection handler using the default port and store the server ID */
    if((error = ts3client_spawnNewServerConnectionHandler(0, &scHandlerID)) != ERROR_ok) {
        wxMessageBox("Error spawning server connection handler");
        return 1;
    }

    /* Get default capture mode */
    if((error = ts3client_getDefaultCaptureMode(&mode)) != ERROR_ok) {
        wxMessageBox("Error getting default capture mode");
        return 1;
    }
	printf("Default capture mode: %s\n", mode);

	/* Get default capture device */
	if((error = ts3client_getDefaultCaptureDevice(mode, &device)) != ERROR_ok) {
		wxMessageBox("Error getting default capture device");
		return 1;
	}
	//wxMessageBox("Default capture device");

    /* Open default capture device */
	/* Instead of passing mode and device[1], it would also be possible to pass empty strings to open the default device */
    if((error = ts3client_openCaptureDevice(scHandlerID, mode, device[1])) != ERROR_ok) {
        wxMessageBox("Error opening capture device");
    }

    /* Get default playback mode */
    if((error = ts3client_getDefaultPlayBackMode(&mode)) != ERROR_ok) {
        wxMessageBox("Error getting default playback mode");
        return 1;
    }
	printf("Default playback mode: %s\n", mode);

	/* Get default playback device */
	if((error = ts3client_getDefaultPlaybackDevice(mode, &device)) != ERROR_ok) {
		wxMessageBox("Error getting default playback device");
		return 1;
	}
	printf("Default playback device: %s %s\n", device[0], device[1]);

    /* Open default playback device */
	/* Instead of passing mode and device[1], it would also be possible to pass empty strings to open the default device */
    if((error = ts3client_openPlaybackDevice(scHandlerID, mode, device[1])) != ERROR_ok) {
        wxMessageBox("Error opening playback device");
    }

    /* Try reading identity from file, otherwise create new identity */
    if(readIdentity(identity) != 0) {
        char* id;
        if((error = ts3client_createIdentity(&id)) != ERROR_ok) {
            wxMessageBox("Error creating identity");
            return 0;
        }
        if(strlen(id) >= IDENTITY_BUFSIZE) {
            wxMessageBox("Not enough bufsize for identity string");
            return 0;
        }
        strcpy(identity, id);
        ts3client_freeMemory(id);
        writeIdentity(identity);
    }
    printf("Using identity: %s\n", identity);

    /* Connect to server on localhost:9987 with nickname "client", no default channel, no default channel password and server password "secret" */
	if((error = ts3client_startConnection(scHandlerID, identity, SERVER_ADDRESS, PORT, NICK, NULL, "", "secret")) != ERROR_ok) {
		wxMessageBox("Error connecting to server");
		return 1;
	}

	printf("Client lib initialized and running\n");

	/* Query and print client lib version */
    if((error = ts3client_getClientLibVersion(&version)) != ERROR_ok) {
        wxMessageBox("Failed to get clientlib version");
        return 1;
    }
    printf("Client lib version: %s\n", version);
    ts3client_freeMemory(version);  /* Release dynamically allocated memory */
    version = NULL;

    SLEEP(300);
    
    //wxMessageBox("Connessione avvenuta con successo!");
    /* Simple commandline interface */
    printf("\nTeamSpeak 3 client commandline interface\n");
    showHelp();

    while(!flag) {

        int c = getc(stdin);
        switch(c) {
            case 'q':
                printf("\nDisconnecting from server...\n");
                flag = 1;
                break;
            case 'h':
                showHelp();
                break;
            case 'c':
                showChannels(DEFAULT_VIRTUAL_SERVER);
                break;
            case 'l':
                showClients(DEFAULT_VIRTUAL_SERVER);
                break;
            case 'L':
            {
                uint64 channelID = enterChannelID();
                if(channelID > 0)
                    showChannelClients(DEFAULT_VIRTUAL_SERVER, channelID);
                break;
            }
            case 'n':
            {
                char name[NAME_BUFSIZE];
                createDefaultChannelName(name);
                createChannel(DEFAULT_VIRTUAL_SERVER, name);
                break;
            }
            case 'N':
            {
                char name[NAME_BUFSIZE];
                emptyInputBuffer();
                enterName(name);
                createChannel(DEFAULT_VIRTUAL_SERVER, name);
                break;
            }
            case 'd':
                deleteChannel(DEFAULT_VIRTUAL_SERVER);
                break;
            case 'r':
                renameChannel(DEFAULT_VIRTUAL_SERVER);
                break;
			case 'R':
				toggleRecordSound(DEFAULT_VIRTUAL_SERVER);
				break;
            case 's':
                switchChannel(DEFAULT_VIRTUAL_SERVER);
                break;
            case 'v':
                toggleVAD(DEFAULT_VIRTUAL_SERVER);
                break;
            case 'V':
                setVadLevel(DEFAULT_VIRTUAL_SERVER);
				break;
			case 'w':
				requestWhisperList(DEFAULT_VIRTUAL_SERVER);
				break;
			case 'W':
				requestClearWhisperList(DEFAULT_VIRTUAL_SERVER);
            /*case 't':
                anyID clientID;
                int n;
                if (ts3client_requestSendChannelTextMsg(DEFAULT_VIRTUAL_SERVER,"Ciao Belli!",(uint64)1,"")==ERROR_ok)
                {
                   printf("Messaggio %s Inviato.\n","Ciao Belli!");
                }
                printf("\nInserisci L'id del client destinatario: ");
                n = scanf("%hu", &clientID);
                emptyInputBuffer();
                if(n == 0)
                {
                    printf("Invalid input. Please enter a number.\n\n");
                }
                printf("%lu",clientID);
                if(ts3client_requestSendPrivateTextMsg(DEFAULT_VIRTUAL_SERVER,"Ciao Mondo!",clientID,"")==ERROR_ok)
                {
                printf("Messaggio %s Inviato.\n","Ciao Mondo!");
                }
                break;*/

        }

        SLEEP(50);
    }

	/* Disconnect from server */
    if((error = ts3client_stopConnection(scHandlerID, "leaving")) != ERROR_ok) {
        printf("Error stopping connection: %d\n", error);
        return 1;
    }

	SLEEP(300);

	/* Destroy server connection handler */
    if((error = ts3client_destroyServerConnectionHandler(scHandlerID)) != ERROR_ok) {
        printf("Error destroying clientlib: %d\n", error);
        return 1;
    }

	/* Shutdown client lib */
    if((error = ts3client_destroyClientLib()) != ERROR_ok) {
        printf("Failed to destroy clientlib: %d\n", error);
        return 1;
    }

	/* This is a small hack, to close an open recording sound file */
	recordSound = 0;
	onEditMixedPlaybackVoiceDataEvent(DEFAULT_VIRTUAL_SERVER, NULL, 0, 0, NULL, NULL);
	return 0;
}

//Do not add custom headers between
//Header Include Start and Header Include End
//wxDev-C++ designer will remove them
////Header Include Start
////Header Include End

//----------------------------------------------------------------------------
// ClientTsFrm
//----------------------------------------------------------------------------
//Add Custom Events only in the appropriate block.
//Code added in other places will be removed by wxDev-C++
////Event Table Start
BEGIN_EVENT_TABLE(ClientTsFrm,wxFrame)
	////Manual Code Start
	////Manual Code End
	
	EVT_CLOSE(ClientTsFrm::OnClose)
	EVT_TIMER(ID_WXTIMER2,ClientTsFrm::WxTimer2Timer)
	EVT_TIMER(ID_WXTIMER1,ClientTsFrm::WxTimer1Timer)
	EVT_BUTTON(ID_WXBUTTON3,ClientTsFrm::txttranslateClick)
	EVT_BUTTON(ID_WXBUTTON2,ClientTsFrm::txtsendClick)
	EVT_TEXT_ENTER(ID_WXEDIT3,ClientTsFrm::txtmsgEnter)
	EVT_BUTTON(ID_WXBUTTON1,ClientTsFrm::WxButton1Click)
END_EVENT_TABLE()
////Event Table End

ClientTsFrm::ClientTsFrm(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &position, const wxSize& size, long style)
: wxFrame(parent, id, title, position, size, style)
{
	CreateGUIControls();
}

ClientTsFrm::~ClientTsFrm()
{
}


void ClientTsFrm::CreateGUIControls()
{
	//Do not add custom code between
	//GUI Items Creation Start and GUI Items Creation End
	//wxDev-C++ designer will remove them.
	//Add the custom code before or after the blocks
	////GUI Items Creation Start

	WxTimer2 = new wxTimer();
	WxTimer2->SetOwner(this, ID_WXTIMER2);
	WxTimer2->Start(500);

	WxTimer1 = new wxTimer();
	WxTimer1->SetOwner(this, ID_WXTIMER1);
	WxTimer1->Start(4000);

	txttranslate = new wxButton(this, ID_WXBUTTON3, _("ITA -> ENG"), wxPoint(120, 384), wxSize(83, 49), 0, wxDefaultValidator, _("txttranslate"));
	txttranslate->Show(false);
	txttranslate->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	txtclient = new wxRichTextCtrl(this, ID_WXRICHTEXTCTRL1, _(""), wxPoint(10, 75), wxSize(184, 155), wxRE_READONLY, wxDefaultValidator, _("txtclient"));
	txtclient->SetMaxLength(0);
	txtclient->SetFocus();
	txtclient->SetInsertionPointEnd();
	txtclient->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	txtlingua = new wxTextCtrl(this, ID_WXEDIT2, _(""), wxPoint(367, 20), wxSize(103, 20), wxTE_READONLY, wxDefaultValidator, _("txtlingua"));
	txtlingua->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	lbllingua = new wxStaticText(this, ID_WXSTATICTEXT2, _("Linguaggio:"), wxPoint(299, 20), wxDefaultSize, 0, _("lbllingua"));
	lbllingua->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	lblnick = new wxStaticText(this, ID_WXSTATICTEXT1, _("Nickname:"), wxPoint(14, 20), wxDefaultSize, 0, _("lblnick"));
	lblnick->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	txtnick = new wxTextCtrl(this, ID_WXEDIT1, _(""), wxPoint(91, 20), wxSize(102, 20), wxTE_READONLY, wxDefaultValidator, _("txtnick"));
	txtnick->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	txtchat = new wxRichTextCtrl(this, ID_WXRICHTEXTCTRL2, _(""), wxPoint(211, 72), wxSize(432, 299), 0, wxDefaultValidator, _("txtchat"));
	txtchat->SetMaxLength(0);
	txtchat->SetFocus();
	txtchat->SetInsertionPointEnd();
	txtchat->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	txtsend = new wxButton(this, ID_WXBUTTON2, _("Invia"), wxPoint(543, 387), wxSize(103, 48), 0, wxDefaultValidator, _("txtsend"));
	txtsend->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	txtmsg = new wxTextCtrl(this, ID_WXEDIT3, _(""), wxPoint(215, 387), wxSize(313, 45), wxTE_PROCESS_ENTER, wxDefaultValidator, _("txtmsg"));
	txtmsg->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	WxButton1 = new wxButton(this, ID_WXBUTTON1, _("Avvia Client"), wxPoint(228, 439), wxSize(149, 31), 0, wxDefaultValidator, _("WxButton1"));
	WxButton1->Show(false);
	WxButton1->SetFont(wxFont(8, wxSWISS, wxNORMAL, wxNORMAL, false));

	SetTitle(_("ClientTs"));
	SetIcon(wxNullIcon);
	SetSize(8,8,682,517);
	Center();
	
	////GUI Items Creation End
	
	conta=10.0;
	FILE*config;
	FILE *api;
	if (config = fopen("..\\conf\\config.txt", "r"))
	{
    fscanf(config,"%s",&SERVER_ADDRESS);
    fscanf(config,"%s",&NICK);
    fscanf(config,"%d",&cmbel);
    fscanf(config,"%s",&LINGUA);
    fscanf(config,"%s",&SERVIZIO);
    fclose(config);
	}
	if (api = fopen("..\\conf\\API.txt", "r"))
	{
		fscanf(api, "%s", API_KEY);
		fclose(api);
	}

	txtnick->AppendText(NICK);
	txtlingua->AppendText(LINGUA);
	HANDLE myHandle = CreateThread(0, 0, myThread, NULL, 0, &myThreadID);
	SetupColor();
	engine = irrklang::createIrrKlangDevice();
	recorder = irrklang::createIrrKlangAudioRecorder(engine);
	
}

void ClientTsFrm::OnClose(wxCloseEvent& event)
{
    flag=1;
    Sleep(300);
	//WxTimer1->Stop();
	//WxTimer1->~wxTimer();
	//WxTimer2->Stop();
	//WxTimer2->~wxTimer();
	Destroy();
}

DWORD WINAPI riceve(LPVOID lpParameter)
{
    /*anyID targetMode;
    anyID toID;
    anyID fromID; 
    const char* fromName;
    const char* fromUniqueIdentifier;
    char* message="";
    while(strcmp(message,"")!=0)
    {
        strcpy(message,"");
        onTextMessageEvent(DEFAULT_VIRTUAL_SERVER,targetMode,toID,fromID,fromName,fromUniqueIdentifier,message);
        message[strlen(message)+1]='\0';
    }*/
    /*
    char benvenuto[30];
     FILE*prova=    fopen("file.txt","w");
    strcpy(benvenuto,"");
	while(1)
    {
    recv(sock,benvenuto,30,0);
    fprintf(prova,"%s\n",benvenuto);
    fflush(prova);
	puts(benvenuto);
	//MessageBox(NULL,(LPCWSTR)benvenuto,(LPCWSTR)"Benvenuto",NULL);
	fflush(stdin);
	fflush(stdout);
	Sleep(500);
    }*/
    return 0;
}



void ClientTsFrm::RefreshChat()
{
    int i;
	wxString *nick=new wxString("");
    wxUniChar ch=':';
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%d-%m-%Y.%X", &tstruct);
    int coloreA=tstruct.tm_sec;
	int coloreB=tstruct.tm_min;
	int coloreC=tstruct.tm_hour;
    showClients(DEFAULT_VIRTUAL_SERVER);
    txtclient->Clear();
    for (i=0;i<MAX;i++)
    {
        if(nomeClient[i]!="") 
        {
            coloreA=coloreB+coloreC%256*(i+1);
            coloreB=coloreA+171*(i+1)%256;
            coloreC=coloreA+coloreB*(i+1)%256;
            txtclient->BeginTextColour(wxColour(colori[i].red, colori[i].green, colori[i].blue));
            txtclient->WriteText(nomeClient[i]);
            txtclient->EndTextColour();
            txtclient->Newline();
        }
    }
        if(strGlobale!="" /*&& strGlobale!=oldstrGlobale*/)
    {
         
        if(strNick==NICK)
        {
            txtchat->WriteText("\nMe\t\t\t\t\t\t");
            txtchat->WriteText("(");
            txtchat->WriteText(buf);
            txtchat->WriteText("): \n");
            txtchat->WriteText(MSG_SRC);
            txtchat->Newline();
        }
        else
        {
            for (i=0;i<MAX;i++)
            {
                if(strNick==persona[i].nome && persona[i].usato==1)
                {
                    //wxMessageBox("Nome: "+persona[i].nome+" Colore : " + wxString::Format(wxT("%i"),persona[i].colore));
                    //wxString prova;
                    //prova="TTS.jar "+strMessage;
                    
                    
                    txtchat->BeginTextColour(wxColour(colori[persona[i].colore].red, colori[persona[i].colore].green, colori[persona[i].colore].blue));
                    txtchat->WriteText("\n"+strNick);
                    txtchat->WriteText("\t\t\t\t\t\t(");
                    txtchat->WriteText(buf);
                    txtchat->WriteText("): ");
                    txtchat->Newline();
                    txtchat->WriteText(/*strMessage+"\n"+*/StringTranslate);
                    txtchat->EndTextColour();
                    //txtchat->Newline();
                    oldstrGlobale=strGlobale;
                    strGlobale="";
                    //system((const char*)prova.mb_str());
                    //wxMessageBox(wxString::FromUTF8((const char*)prova.mb_str()));
                    //Sleep(300);
                    //system("ffmpeg -f s16le -ar 8.0k -ac 1 -i testo.wav");
                    //Sleep(300);
                    //system("testo.wav");
					
                    return;
                }
            }
        }
        //wxMessageBox(strGlobale.BeforeFirst(ch,nick));
        oldstrGlobale=strGlobale;
        strGlobale="";
    }
       
}


/*
 * WxButton1Click
 */
void ClientTsFrm::WxButton1Click(wxCommandEvent& event)
{
    /*iresult=WSAStartup(MAKEWORD(2,2),&wsadata);
	if (iresult!=0) puts("Errore wsadata.\n");
	 if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		puts("Socket fallita.\n");
	}

	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	client_addr.sin_port=htons(1234);
	client_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
	client_addr.sin_family=AF_INET;

	if (connect(sock, (struct sockaddr*)&client_addr, sizeof(struct sockaddr_in)) < 0)
	{
		puts("Errore connetti.\n");
	}
	*/
//    HANDLE myHandle = CreateThread(0, 0, myThread, NULL, 0, &myThreadID);
	//HANDLE myHandle2 = CreateThread(0, 0, riceve, NULL, 0, &myThreadID2);
	// insert your code here
}

/*
 * txtsendClick
 */
void ClientTsFrm::txtsendClick(wxCommandEvent& event)
{
	
    char str[1024]={""};
      strcpy(str,(const char*)txtmsg->GetValue().mb_str(wxConvUTF8));
      
      /*wxString parsata=txtmsg->GetValue();
      parsata.Replace(" ","%20",true);
      parsata.Replace(",","%2C",true);
      wxMessageBox(parsata);*/
     //wxMessageBox(wxString::FromUTF8(buffer2));
    //parse(richiesta(buffer,LINGUA_MSG_SRC));
    //wxString buffer3=wxString::FromUTF8(buffer2);
    if(strcmp(LINGUA,"Italiano")==0) ts3client_requestSendChannelTextMsg(DEFAULT_VIRTUAL_SERVER,"\nItaliano\n"+txtmsg->GetValue()/*+"\nENG: "+StringTranslate*/,(uint64)1,NULL);
    else if(strcmp(LINGUA,"Inglese")==0) ts3client_requestSendChannelTextMsg(DEFAULT_VIRTUAL_SERVER,"\nInglese\n"+txtmsg->GetValue()/*+"\nITA: "+StringTranslate*/,(uint64)1,NULL);
    else if(strcmp(LINGUA,"Portoghese")==0) ts3client_requestSendChannelTextMsg(DEFAULT_VIRTUAL_SERVER,"\nPortoghese\n"+txtmsg->GetValue()/*+"\nITA: "+StringTranslate*/,(uint64)1,NULL);
    
    /*char command[512];
    strcpy(command,"espeak.exe -v it ");
    strcat(command,str);
    system(command);*/
    //aggiorna(strGlobale);
    //toggleRecordSound(DEFAULT_VIRTUAL_SERVER);
    //MessageBox(NULL,txtmsg->GetValue(),NULL,NULL);
	// insert your code here
}

void ClientTsFrm::aggiorna(wxString testo)
{
    txtchat->WriteText(testo);
}

/*
 * txtchatEnter
 */
void ClientTsFrm::txtchatEnter(wxCommandEvent& event)
{
	// insert your code here
	aggiorna(strGlobale);
}

/*
 * WxTimer1Timer
 */
void ClientTsFrm::WxTimer1Timer(wxTimerEvent& event)
{
	// insert your code here
	//if(conta<0) return;
	//wxMessageBox(wxString::FromDouble(conta--));
	//wxMessageBox(wxString::FromUTF8(url));
	RefreshChat();
}

/*
 * txtmsgEnter
 */
void ClientTsFrm::txtmsgEnter(wxCommandEvent& event)
{
	// insert your code here
	//wxMessageBox("Hai scritto "+txtmsg->GetValue()+" nella TextBox!");
	//ts3client_requestSendChannelTextMsg(DEFAULT_VIRTUAL_SERVER,txtmsg->GetValue(),(uint64)1,NULL);
	txtsendClick(event);
}

/*
 * txttranslateClick
 */
void ClientTsFrm::txttranslateClick(wxCommandEvent& event)
{
	// insert your code here
      //const char *str=richiesta((const char*)txtmsg->GetValue().mb_str(wxConvUTF8));
      //char str[]="babylonTranslator.callback('babylon.0.2._babylon_api_response', {\"translatedText\":\"building, construction, edifice, house, structure\"}, 200, null, null);";
      
      /*char str[1024]={""};
      strcpy(str,(const char*)txtmsg->GetValue().mb_str(wxConvUTF8));
      char * pch;
      char buffer[2048]={""};
      pch = strtok (str," ,.-");
      while (pch != NULL)
      {
        strcat(buffer,pch);
        strcat(buffer,"%20");
        printf ("%s\n",pch);
        pch = strtok (NULL, " ,.-");
      }
  parse(richiesta(buffer));*/
  //wxMessageBox(StringTranslate);
    //wxMessageBox(wxString::FromUTF8(parse(richiesta(buffer))));
  //wxMessageBox(wxString::FromUTF8(parse(str)));
  /*wxMessageBox(wxString::FromUTF8(pch2));
  wxMessageBox(wxString::FromUTF8(pch3));*/
}

/*
 * ClientTsFrmActivate
 */
void ClientTsFrm::ClientTsFrmActivate(wxActivateEvent& event)
{
	// insert your code here
}

/*
 * WxButton2Click
 */
void ClientTsFrm::WxButton2Click(wxCommandEvent& event)
{
	// insert your code here
	wxMessageBox(wxString::FromUTF8(url));
}

/*
 * WxTimer2Timer
 */
void ClientTsFrm::WxTimer2Timer(wxTimerEvent& event)
{
	// insert your code here
	 txtchat->ScrollIntoView(txtchat->GetCaretPosition(),WXK_PAGEDOWN);
}
