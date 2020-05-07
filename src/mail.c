/* ************************************************************************
*   File: mail.c                                        Part of CircleMUD *
*  Usage: Internal funcs and player spec-procs of mud-mail system         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/******* MUD MAIL SYSTEM MAIN FILE ***************************************

Written by Jeremy Elson (jelson@circlemud.org)

*************************************************************************/

/* And completely rewritten by Welcor 16th of december, 2005 */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "interpreter.h"
#include "handler.h"
#include "mail.h"


/* external variables */
extern int no_mail;
extern struct player_index_element *player_table;

/* external functions */
SPECIAL(postmaster);

/* local functions */
void postmaster_send_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
void postmaster_check_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
void postmaster_receive_mail(struct char_data *ch, struct char_data *mailman, int cmd, char *arg);
int mail_recip_ok(const char *name);

/* -------------------------------------------------------------------------- */
char *decrypt_hex(char *string, size_t len) 
{
	static char output[MAX_STRING_LENGTH];
	char *p;
	char *src = string;
	int i;
	
	p = output;
	for (i = 0;i<len/2;++i) {
		unsigned char hi = *src++;
		unsigned char lo = *src++;
		hi -= (hi<'A' ? '0' : 'A'-10);
		lo -= (lo<'A' ? '0' : 'A'-10);
		*p++ = (hi<<4) | (lo & 0x0F);
  }
  return output;
}

char *encrypt_hex(char *string, size_t len) 
{
	static char output[MAX_STRING_LENGTH];
	char *p;
  char *src = string;
  int i;
  
	if (len == 0)
		return "";

  p = output;
	for (i=0 ; i<len; i++) {
    unsigned char lo=*src++;
    unsigned char hi=lo>>4;
    lo&=0x0F;
    *p++ = hi+(hi>9 ? 'A'-10 : '0');
    *p++ = lo+(lo>9 ? 'A'-10 : '0');
  }
  return output;
}



int mail_recip_ok(const char *name)
{
  int player_i, ret = FALSE;

  if ((player_i = get_ptable_by_name(name)) >= 0) {
    if (!IS_SET(player_table[player_i].flags, PINDEX_DELETED))
      ret = TRUE;
  }
  return ret;
}


void free_mail_record(struct mail_t *record)
{
	if (record->body)
		free(record->body);
  free(record);
}

struct mail_t *read_mail_record(FILE *mail_file) 
{
  char line[READ_SIZE];
  long sender, recipient;
  time_t sent_time;
  struct mail_t *record;
  
  if (!get_line(mail_file, line))
  	return NULL;
  
  if (sscanf(line, "### %ld %ld %ld", &recipient, &sender, &sent_time) != 3) {
  	log("Mail system - fatal error - malformed mail header");
  	log("Line was: %s", line);
  	return NULL; 
  }
  
  CREATE(record, struct mail_t, 1);
  
  record->recipient = recipient;
  record->sender = sender;
  record->sent_time = sent_time;
  record->body = fread_string(mail_file, "read mail record");
  
  return record;
}

void write_mail_record(FILE *mail_file, struct mail_t *record)
{
	fprintf(mail_file, "### %ld %ld %ld\n"
	                   "%s~\n",
                     record->recipient,
                     record->sender,
                     record->sent_time,
                     record->body );
}
	                   
/*
 * int scan_file(none)
 * Returns false if mail file is corrupted or true if everything correct.
 *
 * This is called once during boot-up.  It scans through the mail file
 * and indexes all entries currently in the mail file.
 */
int scan_file(void)
{
  FILE *mail_file;
  int count = 0;
  struct mail_t *record;

  if (!(mail_file = fopen(MAIL_FILE, "r"))) {
    log("   Mail file non-existant... creating new file.");
    touch(MAIL_FILE);
    return TRUE;
  }
  
  record = read_mail_record(mail_file);
  
  while (record) {
    free_mail_record(record);
    record = read_mail_record(mail_file);
    count++;
  }

  fclose(mail_file);
 	log("   Mail file read -- %d messages.", count);
 	return TRUE;
}

/*
 * int has_mail(long #1)
 * #1 - id number of the person to check for mail.
 * Returns true or false.
 *
 * A simple little function which tells you if the guy has mail or not.
 */
int has_mail(long recipient)
{
  FILE *mail_file;
  struct mail_t *record;
  
  if (!(mail_file = fopen(MAIL_FILE, "r"))) {
    perror("read_delete: Mail file not accessible.");
    return FALSE;
  }
  
  record = read_mail_record(mail_file);
  
  while (record) {
  	if (record->recipient == recipient) {
  		free_mail_record(record);
  		fclose(mail_file);
  		return TRUE;
    }
    free_mail_record(record);
    record = read_mail_record(mail_file);
  }
  fclose(mail_file);
  return FALSE;
}


/*
 * void store_mail(long #1, long #2, char * #3)
 * #1 - id number of the person to mail to.
 * #2 - id number of the person the mail is from.
 * #3 - The actual message to send.
 *
 * call store_mail to store mail.  (hard, huh? :-) )  Pass 3 arguments:
 * who the mail is to (long), who it's from (long), and a pointer to the
 * actual message text (char *).
 */
void store_mail(long to, long from, char *message_pointer)
{
  FILE *mail_file;
  struct mail_t *record;
  
  if (!(mail_file = fopen(MAIL_FILE, "a"))) {
    perror("store_mail: Mail file not accessible.");
    return;
  }
  CREATE(record, struct mail_t, 1);
  
  record->recipient = to;
  record->sender = from;
  record->sent_time = time(0);
  record->body = message_pointer;

  write_mail_record(mail_file, record);
  free(record); /* don't free the body */
  fclose(mail_file);
}

	                    
/*
 * char *read_delete(long #1)
 * #1 - The id number of the person we're checking mail for.
 * Returns the message text of the mail received.
 *
 * Retrieves one messsage for a player. The mail is then discarded from
 * the file.
 *
 * Expects mail to exist.
 */
char *read_delete(long recipient)
{
  FILE *mail_file, *new_file;
  struct mail_t *record, *record_to_keep = NULL;
  char buf[MAX_STRING_LENGTH];
  
  if (!(mail_file = fopen(MAIL_FILE, "r"))) {
    perror("read_delete: Mail file not accessible.");
    return strdup("Mail system malfunction - please report this");
  }

  if (!(new_file = fopen(MAIL_FILE_TMP, "w"))) {
    perror("read_delete: new Mail file not accessible.");
    fclose(mail_file);
    return strdup("Mail system malfunction - please report this");
  }
  
  record = read_mail_record(mail_file);
  
  while (record) {
  	if (!record_to_keep && record->recipient == recipient) {
  		record_to_keep = record;
  		record = read_mail_record(mail_file);
  		continue; /* don't write and free this one just yet */
    }
    write_mail_record(new_file, record);
    free_mail_record(record);
    record = read_mail_record(mail_file);
  }
  
  if (!record_to_keep)
  	sprintf(buf, "Mail system error - please report");
  else {  	
    char *tmstr, *from, *to;

    tmstr = asctime(localtime(&record_to_keep->sent_time));
    *(tmstr + strlen(tmstr) - 1) = '\0';

    from = get_name_by_id(record_to_keep->sender);
    to = get_name_by_id(record_to_keep->recipient);

 		snprintf(buf, sizeof(buf), 
             " * * * * Midgaard Mail System * * * *\r\n"
             "Date: %s\r\n"
             "To  : %s\r\n"
             "From: %s\r\n"
             "\r\n"
             "%s",
   
             tmstr,
             to ? to : "Unknown",
             from ? from : "Unknown",
             record_to_keep->body ? record_to_keep->body : "No message" );
       
    free_mail_record(record_to_keep);
  } 
  fclose(mail_file);
  fclose(new_file);

  remove(MAIL_FILE);
  rename(MAIL_FILE_TMP, MAIL_FILE);
  
  return strdup(buf);
}


/****************************************************************
* Below is the spec_proc for a postmaster using the above       *
* routines.  Written by Jeremy Elson (jelson@circlemud.org) *
****************************************************************/

SPECIAL(postmaster)
{
  if (!ch->desc || IS_NPC(ch))
    return (0);			/* so mobs don't get caught here */

  if (!(CMD_IS("mail") || CMD_IS("check") || CMD_IS("receive")))
    return (0);

  if (no_mail) {
    send_to_char(ch, "Sorry, the mail system is having technical difficulties.\r\n");
    return (0);
  }

  if (CMD_IS("mail")) {
    postmaster_send_mail(ch, (struct char_data *)me, cmd, argument);
    return (1);
  } else if (CMD_IS("check")) {
    postmaster_check_mail(ch, (struct char_data *)me, cmd, argument);
    return (1);
  } else if (CMD_IS("receive")) {
    postmaster_receive_mail(ch, (struct char_data *)me, cmd, argument);
    return (1);
  } else
    return (0);
}


void postmaster_send_mail(struct char_data *ch, struct char_data *mailman,
			  int cmd, char *arg)
{
  long recipient;
  char buf[MAX_INPUT_LENGTH], **mailwrite;

  if (GET_LEVEL(ch) < MIN_MAIL_LEVEL) {
    snprintf(buf, sizeof(buf), "$n tells you, 'Sorry, you have to be level %d to send mail!'", MIN_MAIL_LEVEL);
    act(buf, FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  one_argument(arg, buf);

  if (!*buf) {			/* you'll get no argument from me! */
    act("$n tells you, 'You need to specify an addressee!'",
	FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  if (GET_GOLD(ch) < STAMP_PRICE && GET_LEVEL(ch) < LVL_IMMORT) {
    snprintf(buf, sizeof(buf), "$n tells you, 'A stamp costs %d coin%s.'\r\n"
	    "$n tells you, '...which I see you can't afford.'", STAMP_PRICE,
            STAMP_PRICE == 1 ? "" : "s");
    act(buf, FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  if ((recipient = get_id_by_name(buf)) < 0 || !mail_recip_ok(buf)) {
    act("$n tells you, 'No one by that name is registered here!'",
	FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  act("$n starts to write some mail.", TRUE, ch, 0, 0, TO_ROOM);
  snprintf(buf, sizeof(buf), "$n tells you, 'I'll take %d coins for the stamp.'\r\n"
       "$n tells you, 'Write your message. (/s saves /h for help).'",
	  STAMP_PRICE);

  act(buf, FALSE, mailman, 0, ch, TO_VICT);
  
  if (GET_LEVEL(ch) < LVL_IMMORT)
    GET_GOLD(ch) -= STAMP_PRICE;

  SET_BIT(PLR_FLAGS(ch), PLR_MAILING);	/* string_write() sets writing. */

  /* Start writing! */
  CREATE(mailwrite, char *, 1);
  string_write(ch->desc, mailwrite, MAX_MAIL_SIZE, recipient, NULL);
}


void postmaster_check_mail(struct char_data *ch, struct char_data *mailman,
			  int cmd, char *arg)
{
  if (has_mail(GET_IDNUM(ch)))
    act("$n tells you, 'You have mail waiting.'", FALSE, mailman, 0, ch, TO_VICT);
  else
    act("$n tells you, 'Sorry, you don't have any mail waiting.'", FALSE, mailman, 0, ch, TO_VICT);
}


void postmaster_receive_mail(struct char_data *ch, struct char_data *mailman,
			  int cmd, char *arg)
{
  char buf[256];
  struct obj_data *obj;

  if (!has_mail(GET_IDNUM(ch))) {
    snprintf(buf, sizeof(buf), "$n tells you, 'Sorry, you don't have any mail waiting.'");
    act(buf, FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  while (has_mail(GET_IDNUM(ch))) {
    obj = read_object(1, VIRTUAL); /*a pair of wings will work :)*/ 
    obj->name = strdup("mail paper letter");
    obj->short_description = strdup("a piece of mail");
    obj->description = strdup("Someone has left a piece of mail here.");

    GET_OBJ_TYPE(obj) = ITEM_NOTE;
    GET_OBJ_WEAR(obj) = ITEM_WEAR_TAKE;
    GET_OBJ_WEIGHT(obj) = 1;
    GET_OBJ_COST(obj) = 30;
    GET_OBJ_RENT(obj) = 10;
    obj->action_description = read_delete(GET_IDNUM(ch));

    if (obj->action_description == NULL)
      obj->action_description =
	strdup("Mail system error - please report.  Error #11.\r\n");

    obj_to_char(obj, ch);

    act("$n gives you a piece of mail.", FALSE, mailman, 0, ch, TO_VICT);
    act("$N gives $n a piece of mail.", FALSE, ch, 0, mailman, TO_ROOM);
  }
}
