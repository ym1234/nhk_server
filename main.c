// mostly copied from the samples in eb/samples/
// Comments as is
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iconv.h>
#include <string.h>
#include <errno.h>

#include <eb/eb.h>
#include <eb/error.h>
#include <eb/text.h>

#include "conv.c"


void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void read_write_binary(char *filename, EB_Book *book) {
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  static char buff[1024];
  ssize_t len = 1024;
  while (len == 1024) {
    EB_Error_Code ecode = eb_read_binary(book, 1024, buff, &len);
    if (ecode != EB_SUCCESS) {
      die("eb_read_binary");
    }
    write(fd, buff, len);
  }
  fsync(fd);
  close(fd);
}

EB_Error_Code hook_begin_wave(EB_Book *book, EB_Appendix*, void *data, EB_Hook_Code, int argc, const unsigned int* argv)
{
  if (argc < 6)
    die("DIE DIE DIE\n");

  EB_Position spos = {
    .page = argv[2],
    .offset = argv[3]
  };
  EB_Position epos = {
    .page = argv[4],
    .offset = argv[5]
  };
  static char filename[1024];
  /* printf("%dx%d.wav\n", argv[2], argv[3]); */
  int len = snprintf(filename, 1024, "audio/%dx%d.wav", argv[2], argv[3]);

  EB_Error_Code ecode = eb_set_binary_wave(book, &spos, &epos);
  if (ecode != EB_SUCCESS)
    die("eb_set_binary_wave");

  read_write_binary(filename, book);
  eb_write_text_string(book, "\x01");
  eb_write_text_string(book, filename); // Can I use this like this? Does it "own" after I pass to it?
  eb_write_text_string(book, "\x01");
  return EB_SUCCESS;
}

#define MAX_HITS 50
#define MAXLEN_HEADING 127

int main(int argc, char *argv[]) {
  EB_Error_Code error_code;
  EB_Book book;
  EB_Subbook_Code subbook_list[EB_MAX_SUBBOOKS];
  EB_Hit hits[MAX_HITS];
  char heading[MAXLEN_HEADING + 1];
  int i;

  /* コマンド行引数をチェック。*/
  if (argc != 4) {
    fprintf(stderr, "Usage: %s book-path subbook-index word\n",
        argv[0]);
    exit(1);
  }

  /* EB ライブラリと `book' を初期化。*/
  eb_initialize_library();
  eb_initialize_book(&book);

  /* 書籍を `book' に結び付ける。*/
  error_code = eb_bind(&book, argv[1]);
  if (error_code != EB_SUCCESS) {
    fprintf(stderr, "%s: failed to bind the book, %s: %s\n",
        argv[0], eb_error_message(error_code), argv[1]);
    goto die;
  }

  /* 副本の一覧を取得。*/
  int subbook_count;
  error_code = eb_subbook_list(&book, subbook_list, &subbook_count);
  if (error_code != EB_SUCCESS) {
    fprintf(stderr, "%s: failed to get the subbbook list, %s\n",
        argv[0], eb_error_message(error_code));
    goto die;
  }

  /* 副本のインデックスを取得。*/
  int subbook_index = atoi(argv[2]);

  /*「現在の副本 (current subbook)」を設定。*/
  error_code = eb_set_subbook(&book, subbook_list[subbook_index]);
  if (error_code != EB_SUCCESS) {
    fprintf(stderr, "%s: failed to set the current subbook, %s\n",
        argv[0], eb_error_message(error_code));
    goto die;
  }

  EB_Hookset hookset;
  eb_initialize_hookset(&hookset);
  EB_Hook hook = {.code = EB_HOOK_BEGIN_WAVE, .function = hook_begin_wave};
  eb_set_hook(&hookset, &hook);

  /* 単語検索のリクエストを送出。*/
  char *out = utf8_to_eucjp(argv[3]);
  error_code = eb_search_exactword(&book, out);
  if (error_code != EB_SUCCESS) {
    fprintf(stderr, "%s: failed to search for the word, %s: %s\n",
        argv[0], eb_error_message(error_code), argv[3]);
    goto die;
  }

  for (;;) {
    /* 残っているヒットエントリを取得。*/
    int hit_count;
    error_code = eb_hit_list(&book, MAX_HITS, hits, &hit_count);
    if (error_code != EB_SUCCESS) {
      fprintf(stderr, "%s: failed to get hit entries, %s\n",
          argv[0], eb_error_message(error_code));
      goto die;
    }
    if (hit_count == 0)
      break;

    for (i = 0; i < hit_count; i++) {
      /* 見出しの位置へ移動。*/
      error_code = eb_seek_text(&book, &(hits[i].text));
      if (error_code != EB_SUCCESS) {
        fprintf(stderr, "%s: failed to seek the subbook, %s\n",
            argv[0], eb_error_message(error_code));
        goto die;
      }

      /* 見出しを取得して表示。*/
      static char text[1024];
      ssize_t len = 1024;
      int mode = 0; // 0 Search 1 Send
      while (len == 1024) {
        error_code = eb_read_text(&book, NULL, &hookset, NULL, 1024, text, &len);
        if (error_code != EB_SUCCESS) {
          fprintf(stderr, "%s: failed to read the subbook, %s\n",
              argv[0], eb_error_message(error_code));
          goto die;
        }
        for (int i = 0; i < len; ++i) {
          if (mode == 0) {
            if (text[i] == '\x01') mode = 1;
          } else {
            if (text[i] == '\x01') {
              mode = 0;
              printf("\n");
              continue;
            }
            putchar(text[i]);
          }
        }
      }
    }
  }

  /* 書籍と EB ライブラリの利用を終了。*/
  eb_finalize_hookset(&hookset);
  eb_finalize_book(&book);
  eb_finalize_library();
  return 0;

  /* エラー発生で終了するときの処理。*/
die:
  eb_finalize_hookset(&hookset);
  eb_finalize_book(&book);
  eb_finalize_library();
  return 1;
}
