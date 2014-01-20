/*
Copyright (c) 2013, Intel Corporation

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.

David Navarro <david.navarro@intel.com>

*/

#include "internals.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static int prv_parse_number(const char * uriString,
                            size_t len,
                            int * headP)
{
    int result = 0;
    int mul = 0;
    int uriLength = len;

    while (*headP < uriLength && uriString[*headP] != '/')
    {
        if ('0' <= uriString[*headP] && uriString[*headP] <= '9')
        {
            if (0 == mul)
            {
                mul = 10;
            }
            else
            {
                result *= mul;
            }
            result += uriString[*headP] - '0';
        }
        else
        {
            return -1;
        }
        *headP += 1;
    }

    if (uriString[*headP] == '/')
        *headP += 1;

    return result;
}


int prv_get_number(const char * uriString,
                   size_t uriLength)
{
    int index = 0;

    return prv_parse_number(uriString, uriLength, &index);
}


lwm2m_uri_t * lwm2m_decode_uri(multi_option_t *uriPath)
{
    lwm2m_uri_t * uriP;
    int readNum;

    if (NULL == uriPath) return NULL;

    uriP = (lwm2m_uri_t *)lwm2m_malloc(sizeof(lwm2m_uri_t));
    if (NULL == uriP) return NULL;

    memset(uriP, 0, sizeof(lwm2m_uri_t));

    // Read object ID
    if (URI_REGISTRATION_SEGMENT_LEN == uriPath->len
     && 0 == strncmp(URI_REGISTRATION_SEGMENT, uriPath->data, uriPath->len))
    {
        uriP->flag |= LWM2M_URI_FLAG_REGISTRATION;
        uriPath = uriPath->next;
        if (uriPath == NULL) return uriP;
    }
    else if (URI_BOOTSTRAP_SEGMENT_LEN == uriPath->len
     && 0 == strncmp(URI_BOOTSTRAP_SEGMENT, uriPath->data, uriPath->len))
    {
        uriP->flag |= LWM2M_URI_FLAG_BOOTSTRAP;
        uriPath = uriPath->next;
        if (uriPath != NULL) goto error;
        return uriP;
    }

    readNum = prv_get_number(uriPath->data, uriPath->len);
    if (readNum < 0 || readNum > LWM2M_MAX_ID) goto error;
    uriP->objectId = (uint16_t)readNum;
    uriP->flag |= LWM2M_URI_FLAG_OBJECT_ID;
    uriPath = uriPath->next;

    if ((uriP->flag & LWM2M_URI_MASK_TYPE) == LWM2M_URI_FLAG_REGISTRATION)
    {
        if (uriPath != NULL) goto error;
        return uriP;
    }
    uriP->flag |= LWM2M_URI_FLAG_DM;

    if (uriPath == NULL) return uriP;

    // Read object instance
    if (uriPath->len != 0)
    {
        readNum = prv_get_number(uriPath->data, uriPath->len);
        if (readNum < 0 || readNum >= LWM2M_MAX_ID) goto error;
        uriP->instanceId = (uint16_t)readNum;
        uriP->flag |= LWM2M_URI_FLAG_INSTANCE_ID;
    }
    uriPath = uriPath->next;

    if (uriPath == NULL) return uriP;

    // Read ressource ID
    if (uriPath->len != 0)
    {
        readNum = prv_get_number(uriPath->data, uriPath->len);
        if (readNum < 0 || readNum > LWM2M_MAX_ID) goto error;
        uriP->resourceId = (uint16_t)readNum;
        uriP->flag |= LWM2M_URI_FLAG_RESOURCE_ID;
    }

    // must be the last segment
    if (NULL == uriPath->next) return uriP;

error:
    lwm2m_free(uriP);
    return NULL;
}

int lwm2m_stringToUri(char * buffer,
                      size_t len,
                      lwm2m_uri_t * uriP)
{
    int head;
    int readNum;
    int buffer_len = len;

    if (buffer == NULL || buffer_len == 0 || uriP == NULL) return 0;

    // Skip any white space
    head = 0;
    while (head < buffer_len && isspace(buffer[head]))
    {
        head++;
    }
    if (head == buffer_len) return 0;

    // Read object ID
    readNum = prv_parse_number(buffer, buffer_len, &head);
    if (readNum < 0 || readNum > LWM2M_MAX_ID) return 0;
    uriP->objectId = (uint16_t)readNum;

    if (head >= buffer_len) return head;

    // Read object instance
    if (buffer[head] == '/')
    {
        // default instance
        head++;
    }
    else
    {
        readNum = prv_parse_number(buffer, buffer_len, &head);
        if (readNum < 0 || readNum >= LWM2M_MAX_ID) return 0;
        uriP->instanceId = (uint16_t)readNum;
        uriP->flag |= LWM2M_URI_FLAG_INSTANCE_ID;
    }
    if (head >= buffer_len) return head;

    // Read ressource ID
    if (buffer[head] == '/')
    {
        // no ID
        head++;
    }
    else
    {
        readNum = prv_parse_number(buffer, buffer_len, &head);
        if (readNum < 0 || readNum >= LWM2M_MAX_ID) return 0;
        uriP->resourceId = (uint16_t)readNum;
        uriP->flag |= LWM2M_URI_FLAG_RESOURCE_ID;
    }

    return head;
}

