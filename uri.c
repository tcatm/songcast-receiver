#include <uriparser/Uri.h>

#include "uri.h"

char *UriTextRangeString(UriTextRangeA *textrange) {
  return strndup(textrange->first, textrange->afterLast - textrange->first);
}

struct uri *parse_uri(char *uri_string) {
  UriParserStateA state;
  UriUriA uri;

  state.uri = &uri;

  if (uriParseUriA(&state, uri_string) != URI_SUCCESS) {
    uriFreeUriMembersA(&uri);
    error(1, 0, "Could not parse URI");
  }

  struct uri *s = calloc(1, sizeof(struct uri));

  s->scheme = UriTextRangeString(&uri.scheme);
  s->host = UriTextRangeString(&uri.hostText);
  s->port = atoi(UriTextRangeString(&uri.portText));

  if (&uri.pathHead->text != NULL)
    s->path = UriTextRangeString(&uri.pathHead->text);
  else
    s->path = strdup("");

  uriFreeUriMembersA(&uri);

  return s;
}

void free_uri(struct uri *uri) {
  free(uri->scheme);
  free(uri->host);
  free(uri->path);
  free(uri);
}
