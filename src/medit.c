/************************************************************************
 * OasisOLC - Mobiles / medit.c					v2.0	*
 * Copyright 1996 Harvey Gilpin						*
 * Copyright 1997-2001 George Greer (greerga@circlemud.org)		*
 ************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "interpreter.h"
#include "comm.h"
#include "spells.h"
#include "utils.h"
#include "db.h"
#include "shop.h"
#include "genolc.h"
#include "genmob.h"
#include "genzon.h"
#include "genshp.h"
#include "oasis.h"
#include "handler.h"
#include "constants.h"
#include "improved-edit.h"
#include "dg_olc.h"
#include "screen.h"

/*-------------------------------------------------------------------*/

/*
 * External variable declarations.
 */
extern struct attack_hit_type attack_hit_text[];
#if CONFIG_OASIS_MPROG
extern const char *mobprog_types[];
#endif

/*-------------------------------------------------------------------*/

/*
 * Handy internal macros.
 */
#if CONFIG_OASIS_MPROG
#define GET_MPROG(mob)		(mob_index[(mob)->nr].mobprogs)
#define GET_MPROG_TYPE(mob)	(mob_index[(mob)->nr].progtypes)
#endif

/*-------------------------------------------------------------------*/

/*
 * Function prototypes.
 */
#if CONFIG_OASIS_MPROG
void medit_disp_mprog(struct descriptor_data *d);
void medit_change_mprog(struct descriptor_data *d);
const char *medit_get_mprog_type(struct mob_prog_data *mprog);
#endif

/*-------------------------------------------------------------------*\
  utility functions 
\*-------------------------------------------------------------------*/

ACMD(do_oasis_medit)
{
  int number = NOBODY, save = 0, real_num;
  struct descriptor_data *d;
  char *buf3;
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  
  /****************************************************************************/
  /** Parse any arguments.                                                   **/
  /****************************************************************************/
  buf3 = two_arguments(argument, buf1, buf2);
  
  if (!*buf1) {
    send_to_char(ch, "Specify a mobile VNUM to edit.\r\n");
    return;
  } else if (!isdigit(*buf1)) {
    if (str_cmp("save", buf1) != 0) {
      send_to_char(ch, "Yikes!  Stop that, someone will get hurt!\r\n");
      return;
    }
    
    save = TRUE;
    
    if (is_number(buf2))
      number = atoi(buf2);
    else if (GET_OLC_ZONE(ch) > 0) {
      zone_rnum zlok;
      
      if ((zlok = real_zone(GET_OLC_ZONE(ch))) == NOWHERE)
        number = NOWHERE;
      else
        number = genolc_zone_bottom(zlok);
    }
    
    if (number == NOWHERE) {
      send_to_char(ch, "Save which zone?\r\n");
      return;
    }
  }
  
  /****************************************************************************/
  /** If a numeric argument was given (like a room number), get it.          **/
  /****************************************************************************/
  if (number == NOBODY)
    number = atoi(buf1);
  
  /****************************************************************************/
  /** Check that whatever it is isn't already being edited.                  **/
  /****************************************************************************/
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) == CON_MEDIT) {
      if (d->olc && OLC_NUM(d) == number) {
        send_to_char(ch, "That mobile is currently being edited by %s.\r\n",
          GET_NAME(d->character));
        return;
      }
    }
  }
  
  d = ch->desc;
  
  /****************************************************************************/
  /** Give descriptor an OLC structure.                                      **/
  /****************************************************************************/
  if (d->olc) {
    mudlog(BRF, LVL_IMMORT, TRUE,
      "SYSERR: do_oasis_medit: Player already had olc structure.");
    free(d->olc);
  }
  
  CREATE(d->olc, struct oasis_olc_data, 1);
  
  /****************************************************************************/
  /** Find the zone.                                                         **/
  /****************************************************************************/
  OLC_ZNUM(d) = save ? real_zone(number) : real_zone_by_thing(number);
  if (OLC_ZNUM(d) == NOWHERE) {
    send_to_char(ch, "Sorry, there is no zone for that number!\r\n");
    free(d->olc);
    d->olc = NULL;
    return;
  }
  
  /****************************************************************************/
  /** Everyone but IMPLs can only edit zones they have been assigned.        **/
  /****************************************************************************/
  if (!can_edit_zone(ch, OLC_ZNUM(d))) {
    send_to_char(ch, " You do not have permission to edit zone %d. Try zone %d.\r\n", zone_table[OLC_ZNUM(d)].number, GET_OLC_ZONE(ch));
mudlog(BRF, LVL_IMPL, TRUE, "OLC: %s tried to edit zone %d allowed zone %d",
      GET_NAME(ch), zone_table[OLC_ZNUM(d)].number, GET_OLC_ZONE(ch));
    free(d->olc);
    d->olc = NULL;
    return;
  }
  
  /****************************************************************************/
  /** If save is TRUE, save the mobiles.                                     **/
  /****************************************************************************/
  if (save) {
    send_to_char(ch, "Saving all mobiles in zone %d.\r\n",
      zone_table[OLC_ZNUM(d)].number);
    mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE,
      "OLC: %s saves mobile info for zone %d.",
      GET_NAME(ch), zone_table[OLC_ZNUM(d)].number);
    
    /**************************************************************************/
    /** Save the mobiles.                                                    **/
    /**************************************************************************/
    save_mobiles(OLC_ZNUM(d));
    
    /**************************************************************************/
    /** Free the olc structure stored in the descriptor.                     **/
    /**************************************************************************/
    free(d->olc);
    d->olc = NULL;
    return;
  }
  
  OLC_NUM(d) = number;
  
  /****************************************************************************/
  /** If this is a new mobile, setup a new one, otherwise, setup the         **/
  /** existing mobile.                                                       **/
  /****************************************************************************/
  if ((real_num = real_mobile(number)) == NOBODY)
    medit_setup_new(d);
  else
    medit_setup_existing(d, real_num);
  
  STATE(d) = CON_MEDIT;
  
  /****************************************************************************/
  /** Display the OLC messages to the players in the same room as the        **/
  /** builder and also log it.                                               **/
  /****************************************************************************/
  act("$n starts using OLC.", TRUE, d->character, 0, 0, TO_ROOM);
  SET_BIT(PLR_FLAGS(ch), PLR_WRITING);
  
  mudlog(CMP, LVL_IMMORT, TRUE,"OLC: %s starts editing zone %d allowed zone %d",
    GET_NAME(ch), zone_table[OLC_ZNUM(d)].number, GET_OLC_ZONE(ch));
}

void medit_save_to_disk(zone_vnum foo)
{
  save_mobiles(real_zone(foo));
}

void medit_setup_new(struct descriptor_data *d)
{
  struct char_data *mob;

  /*
   * Allocate a scratch mobile structure.  
   */
  CREATE(mob, struct char_data, 1);

  init_mobile(mob);

  GET_MOB_RNUM(mob) = NOBODY;
  /*
   * Set up some default strings.
   */
  GET_ALIAS(mob) = strdup("mob unfinished");
  GET_SDESC(mob) = strdup("the unfinished mob");
  GET_LDESC(mob) = strdup("An unfinished mob stands here.\r\n");
  GET_DDESC(mob) = strdup("It looks unfinished.\r\n");
#if CONFIG_OASIS_MPROG
  OLC_MPROGL(d) = NULL;
  OLC_MPROG(d) = NULL;
#endif
  SCRIPT(mob) = NULL;
  mob->proto_script = OLC_SCRIPT(d) = NULL;
  
  OLC_MOB(d) = mob;
  /* Has changed flag. (It hasn't so far, we just made it.) */
  OLC_VAL(d) = FALSE;
  OLC_ITEM_TYPE(d) = MOB_TRIGGER;
  
  medit_disp_menu(d);
}

/*-------------------------------------------------------------------*/

void medit_setup_existing(struct descriptor_data *d, int rmob_num)
{
  struct char_data *mob;

  /*
   * Allocate a scratch mobile structure. 
   */
  CREATE(mob, struct char_data, 1);

  copy_mobile(mob, mob_proto + rmob_num);

#if CONFIG_OASIS_MPROG
  {
    MPROG_DATA *temp;
    MPROG_DATA *head;

    if (GET_MPROG(mob))
      CREATE(OLC_MPROGL(d), MPROG_DATA, 1);
    head = OLC_MPROGL(d);
    for (temp = GET_MPROG(mob); temp; temp = temp->next) {
      OLC_MPROGL(d)->type = temp->type;
      OLC_MPROGL(d)->arglist = strdup(temp->arglist);
      OLC_MPROGL(d)->comlist = strdup(temp->comlist);
      if (temp->next) {
        CREATE(OLC_MPROGL(d)->next, MPROG_DATA, 1);
        OLC_MPROGL(d) = OLC_MPROGL(d)->next;
      }
    }
    OLC_MPROGL(d) = head;
    OLC_MPROG(d) = OLC_MPROGL(d);
  }
#endif

  OLC_MOB(d) = mob;
  OLC_ITEM_TYPE(d) = MOB_TRIGGER;
  dg_olc_script_copy(d);
  /*
   * The edited mob must not have a script.
   * It will be assigned to the updated mob later, after editing.
   */
  SCRIPT(mob) = NULL;
  OLC_MOB(d)->proto_script = NULL;

  medit_disp_menu(d);
}

/*-------------------------------------------------------------------*/

/*
 * Ideally, this function should be in db.c, but I'll put it here for
 * portability.
 */
void init_mobile(struct char_data *mob)
{
  clear_char(mob);

  GET_HIT(mob) = GET_MANA(mob) = 1;
  GET_MAX_MANA(mob) = GET_MAX_MOVE(mob) = 100;
  GET_NDD(mob) = GET_SDD(mob) = 1;
  GET_WEIGHT(mob) = 200;
  GET_HEIGHT(mob) = 198;

  mob->real_abils.str = mob->real_abils.intel = mob->real_abils.wis = 11;
  mob->real_abils.dex = mob->real_abils.con = mob->real_abils.cha = 11;
  mob->aff_abils = mob->real_abils;

  SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);
  mob->player_specials = &dummy_mob;
}

/*-------------------------------------------------------------------*/

/*
 * Save new/edited mob to memory.
 */
void medit_save_internally(struct descriptor_data *d)
{
  int i;
  mob_rnum new_rnum;
  struct descriptor_data *dsc;
  struct char_data *mob;
  
  i = (real_mobile(OLC_NUM(d)) == NOBODY);
  
  if ((new_rnum = add_mobile(OLC_MOB(d), OLC_NUM(d))) == NOBODY) {
    log("medit_save_internally: add_mobile failed.");
    return;
  }

  

  /* Update triggers */
  /* Free old proto list  */
  if (mob_proto[new_rnum].proto_script &&
      mob_proto[new_rnum].proto_script != OLC_SCRIPT(d)) 
    free_proto_script(&mob_proto[new_rnum], MOB_TRIGGER);   

  mob_proto[new_rnum].proto_script = OLC_SCRIPT(d);

  /* this takes care of the mobs currently in-game */
  for (mob = character_list; mob; mob = mob->next) {
    if (GET_MOB_RNUM(mob) != new_rnum) 
      continue;
    
    /* remove any old scripts */
    if (SCRIPT(mob)) 
      extract_script(mob, MOB_TRIGGER);

    free_proto_script(mob, MOB_TRIGGER);
    copy_proto_script(&mob_proto[new_rnum], mob, MOB_TRIGGER);
    assign_triggers(mob, MOB_TRIGGER);
  }
  /* end trigger update */  

  if (!i)	/* Only renumber on new mobiles. */
    return;

  /*
   * Update keepers in shops being edited and other mobs being edited.
   */
  for (dsc = descriptor_list; dsc; dsc = dsc->next) {
    if (STATE(dsc) == CON_SEDIT)
      S_KEEPER(OLC_SHOP(dsc)) += (S_KEEPER(OLC_SHOP(dsc)) >= new_rnum);
    else if (STATE(dsc) == CON_MEDIT)
      GET_MOB_RNUM(OLC_MOB(dsc)) += (GET_MOB_RNUM(OLC_MOB(dsc)) >= new_rnum);
  }

  /*
   * Update other people in zedit too. From: C.Raehl 4/27/99
   */
  for (dsc = descriptor_list; dsc; dsc = dsc->next)
    if (STATE(dsc) == CON_ZEDIT)
      for (i = 0; OLC_ZONE(dsc)->cmd[i].command != 'S'; i++)
        if (OLC_ZONE(dsc)->cmd[i].command == 'M')
          if (OLC_ZONE(dsc)->cmd[i].arg1 >= new_rnum)
            OLC_ZONE(dsc)->cmd[i].arg1++;
}

/**************************************************************************
 Menu functions 
 **************************************************************************/

/*
 * Display positions. (sitting, standing, etc)
 */
void medit_disp_positions(struct descriptor_data *d)
{
  int i;

  get_char_colors(d->character);
  clear_screen(d);

  for (i = 0; *position_types[i] != '\n'; i++) {
    write_to_output(d, "%s%2d%s) %s\r\n", grn, i, nrm, position_types[i]);
  }
  write_to_output(d, "Enter position number : ");
}

/*-------------------------------------------------------------------*/

#if CONFIG_OASIS_MPROG
/*
 * Get the type of MobProg.
 */
const char *medit_get_mprog_type(struct mob_prog_data *mprog)
{
  switch (mprog->type) {
  case IN_FILE_PROG:	return ">in_file_prog";
  case ACT_PROG:	return ">act_prog";
  case SPEECH_PROG:	return ">speech_prog";
  case RAND_PROG:	return ">rand_prog";
  case FIGHT_PROG:	return ">fight_prog";
  case HITPRCNT_PROG:	return ">hitprcnt_prog";
  case DEATH_PROG:	return ">death_prog";
  case ENTRY_PROG:	return ">entry_prog";
  case GREET_PROG:	return ">greet_prog";
  case ALL_GREET_PROG:	return ">all_greet_prog";
  case GIVE_PROG:	return ">give_prog";
  case BRIBE_PROG:	return ">bribe_prog";
  }
  return ">ERROR_PROG";
}

/*-------------------------------------------------------------------*/

/*
 * Display the MobProgs.
 */
void medit_disp_mprog(struct descriptor_data *d)
{
  struct mob_prog_data *mprog = OLC_MPROGL(d);

  OLC_MTOTAL(d) = 1;

  clear_screen(d);
  while (mprog) {
    write_to_output(d, "%d) %s %s\r\n", OLC_MTOTAL(d), medit_get_mprog_type(mprog),
		(mprog->arglist ? mprog->arglist : "NONE"));
    OLC_MTOTAL(d)++;
    mprog = mprog->next;
  }
  write_to_output(d,  "%d) Create New Mob Prog\r\n"
		"%d) Purge Mob Prog\r\n"
		"Enter number to edit [0 to exit]:  ",
		OLC_MTOTAL(d), OLC_MTOTAL(d) + 1);
  OLC_MODE(d) = MEDIT_MPROG;
}

/*-------------------------------------------------------------------*/

/*
 * Change the MobProgs.
 */
void medit_change_mprog(struct descriptor_data *d)
{
  clear_screen(d);
  write_to_output(d,  "1) Type: %s\r\n"
		"2) Args: %s\r\n"
		"3) Commands:\r\n%s\r\n\r\n"
		"Enter number to edit [0 to exit]: ",
	medit_get_mprog_type(OLC_MPROG(d)),
	(OLC_MPROG(d)->arglist ? OLC_MPROG(d)->arglist: "NONE"),
	(OLC_MPROG(d)->comlist ? OLC_MPROG(d)->comlist : "NONE"));

  OLC_MODE(d) = MEDIT_CHANGE_MPROG;
}

/*-------------------------------------------------------------------*/

/*
 * Change the MobProg type.
 */
void medit_disp_mprog_types(struct descriptor_data *d)
{
  int i;

  get_char_colors(d->character);
  clear_screen(d);

  for (i = 0; i < NUM_PROGS-1; i++) {
    write_to_output(d, "%s%2d%s) %s\r\n", grn, i, nrm, mobprog_types[i]);
  }
  write_to_output(d, "Enter mob prog type : ");
  OLC_MODE(d) = MEDIT_MPROG_TYPE;
}
#endif

/*-------------------------------------------------------------------*/

/*
 * Display the gender of the mobile.
 */
void medit_disp_sex(struct descriptor_data *d)
{
  int i;

  get_char_colors(d->character);
  clear_screen(d);

  for (i = 0; i < NUM_GENDERS; i++) {
    write_to_output(d, "%s%2d%s) %s\r\n", grn, i, nrm, genders[i]);
  }
  write_to_output(d, "Enter gender number : ");
}

/*-------------------------------------------------------------------*/

/*
 * Display attack types menu.
 */
void medit_disp_attack_types(struct descriptor_data *d)
{
  int i;

  get_char_colors(d->character);
  clear_screen(d);

  for (i = 0; i < NUM_ATTACK_TYPES; i++) {
    write_to_output(d, "%s%2d%s) %s\r\n", grn, i, nrm, attack_hit_text[i].singular);
  }
  write_to_output(d, "Enter attack type : ");
}

/*-------------------------------------------------------------------*/

/*
 * Display mob-flags menu.
 */
void medit_disp_mob_flags(struct descriptor_data *d)
{
  int i, columns = 0;
  char flags[MAX_STRING_LENGTH];
  
  get_char_colors(d->character);
  clear_screen(d);
  for (i = 0; i < NUM_MOB_FLAGS; i++) {
    write_to_output(d, "%s%2d%s) %-20.20s  %s", grn, i + 1, nrm, action_bits[i],
		!(++columns % 2) ? "\r\n" : "");
  }
  sprintbit(MOB_FLAGS(OLC_MOB(d)), action_bits, flags, sizeof(flags));
  write_to_output(d, "\r\nCurrent flags : %s%s%s\r\nEnter mob flags (0 to quit) : ",
		  cyn, flags, nrm);
}

/*-------------------------------------------------------------------*/

/*
 * Display affection flags menu.
 */
void medit_disp_aff_flags(struct descriptor_data *d)
{
  int i, columns = 0;
  char flags[MAX_STRING_LENGTH];

  get_char_colors(d->character);
  clear_screen(d);
  for (i = 0; i < NUM_AFF_FLAGS; i++) {
    write_to_output(d, "%s%2d%s) %-20.20s  %s", grn, i + 1, nrm, affected_bits[i],
			!(++columns % 2) ? "\r\n" : "");
  }
  sprintbit(AFF_FLAGS(OLC_MOB(d)), affected_bits, flags, sizeof(flags));
  write_to_output(d, "\r\nCurrent flags   : %s%s%s\r\nEnter aff flags (0 to quit) : ",
			  cyn, flags, nrm);
}

/*-------------------------------------------------------------------*/

/*
 * Display main menu.
 */
void medit_disp_menu(struct descriptor_data *d)
{
  struct char_data *mob;
  char flags[MAX_STRING_LENGTH], flag2[MAX_STRING_LENGTH];

  mob = OLC_MOB(d);
  get_char_colors(d->character);
  clear_screen(d);

  write_to_output(d,
  "-- Mob Number:  [%s%d%s]\r\n"
  "%s1%s) Sex: %s%-7.7s%s	         %s2%s) Alias: %s%s\r\n"
  "%s3%s) S-Desc: %s%s\r\n"
  "%s4%s) L-Desc:-\r\n%s%s"
  "%s5%s) D-Desc:-\r\n%s%s"
  "%s6%s) Level:       [%s%4d%s],  %s7%s) Alignment:    [%s%4d%s]\r\n"
  "%s8%s) Hitroll:     [%s%4d%s],  %s9%s) Damroll:      [%s%4d%s]\r\n"
  "%sA%s) NumDamDice:  [%s%4d%s],  %sB%s) SizeDamDice:  [%s%4d%s]\r\n"
  "%sC%s) Num HP Dice: [%s%4d%s],  %sD%s) Size HP Dice: [%s%4d%s],  %sE%s) HP Bonus: [%s%5d%s]\r\n"
  "%sF%s) Armor Class: [%s%4d%s],  %sG%s) Exp:     [%s%9d%s],  %sH%s) Gold:  [%s%8d%s]\r\n",

	  cyn, OLC_NUM(d), nrm,
	  grn, nrm, yel, genders[(int)GET_SEX(mob)], nrm,
	  grn, nrm, yel, GET_ALIAS(mob),
	  grn, nrm, yel, GET_SDESC(mob),
	  grn, nrm, yel, GET_LDESC(mob),
	  grn, nrm, yel, GET_DDESC(mob),
	  grn, nrm, cyn, GET_LEVEL(mob), nrm,
	  grn, nrm, cyn, GET_ALIGNMENT(mob), nrm,
	  grn, nrm, cyn, GET_HITROLL(mob), nrm,
	  grn, nrm, cyn, GET_DAMROLL(mob), nrm,
	  grn, nrm, cyn, GET_NDD(mob), nrm,
	  grn, nrm, cyn, GET_SDD(mob), nrm,
	  grn, nrm, cyn, GET_HIT(mob), nrm,
	  grn, nrm, cyn, GET_MANA(mob), nrm,
	  grn, nrm, cyn, GET_MOVE(mob), nrm,
	  grn, nrm, cyn, GET_AC(mob), nrm,
	  grn, nrm, cyn, GET_EXP(mob), nrm,
	  grn, nrm, cyn, GET_GOLD(mob), nrm
	  );

  sprintbit(MOB_FLAGS(mob), action_bits, flags, sizeof(flags));
  sprintbit(AFF_FLAGS(mob), affected_bits, flag2, sizeof(flag2));
  write_to_output(d,
	  "%sI%s) Position  : %s%s\r\n"
	  "%sJ%s) Default   : %s%s\r\n"
	  "%sK%s) Attack    : %s%s\r\n"
	  "%sL%s) NPC Flags : %s%s\r\n"
	  "%sM%s) AFF Flags : %s%s\r\n"
#if CONFIG_OASIS_MPROG
	  "%sP%s) Mob Progs : %s%s\r\n"
#endif
          "%sS%s) Script    : %s%s\r\n"
          "%sX%s) Delete mob\r\n"
	  "%sQ%s) Quit\r\n"
	  "Enter choice : ",

	  grn, nrm, yel, position_types[(int)GET_POS(mob)],
	  grn, nrm, yel, position_types[(int)GET_DEFAULT_POS(mob)],
	  grn, nrm, yel, attack_hit_text[(int)GET_ATTACK(mob)].singular,
	  grn, nrm, cyn, flags,
	  grn, nrm, cyn, flag2,
#if CONFIG_OASIS_MPROG
	  grn, nrm, cyn, (OLC_MPROGL(d) ? "Set." : "Not Set."),
#endif
          grn, nrm, cyn, OLC_SCRIPT(d) ?"Set.":"Not Set.",
          grn, nrm,
	  grn, nrm
	  );

  OLC_MODE(d) = MEDIT_MAIN_MENU;
}

/************************************************************************
 *			The GARGANTAUN event handler			*
 ************************************************************************/

void medit_parse(struct descriptor_data *d, char *arg)
{
  int i = -1;
  char *oldtext = NULL;

  if (OLC_MODE(d) > MEDIT_NUMERICAL_RESPONSE) {
    i = atoi(arg);
    if (!*arg || (!isdigit(arg[0]) && ((*arg == '-') && !isdigit(arg[1])))) {
      write_to_output(d, "Field must be numerical, try again : ");
      return;
    }
  } else {	/* String response. */
    if (!genolc_checkstring(d, arg))
      return;
  }
  switch (OLC_MODE(d)) {
/*-------------------------------------------------------------------*/
  case MEDIT_CONFIRM_SAVESTRING:
    /*
     * Ensure mob has MOB_ISNPC set or things will go pear shaped.
     */
    SET_BIT(MOB_FLAGS(OLC_MOB(d)), MOB_ISNPC);
    switch (*arg) {
    case 'y':
    case 'Y':
      /*
       * Save the mob in memory and to disk.
       */
      medit_save_internally(d);
      mudlog(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(d->character)), TRUE, "OLC: %s edits mob %d", GET_NAME(d->character), OLC_NUM(d));
      if (CONFIG_OLC_SAVE) {
        medit_save_to_disk(zone_table[real_zone_by_thing(OLC_NUM(d))].number);
        write_to_output(d, "Mobile saved to disk.\r\n");
      } else
        write_to_output(d, "Mobile saved to memory.\r\n");
      /* FALL THROUGH */
    case 'n':
    case 'N':
      cleanup_olc(d, CLEANUP_ALL);
      return;
    default:
      write_to_output(d, "Invalid choice!\r\n");
      write_to_output(d, "Do you wish to save the mobile? : ");
      return;
    }
    break;

/*-------------------------------------------------------------------*/
  case MEDIT_MAIN_MENU:
    i = 0;
    switch (*arg) {
    case 'q':
    case 'Q':
      if (OLC_VAL(d)) {	/* Anything been changed? */
	write_to_output(d, "Do you wish to save the changes to the mobile? (y/n) : ");
	OLC_MODE(d) = MEDIT_CONFIRM_SAVESTRING;
      } else
	cleanup_olc(d, CLEANUP_ALL);
      return;
    case '1':
      OLC_MODE(d) = MEDIT_SEX;
      medit_disp_sex(d);
      return;
    case '2':
      OLC_MODE(d) = MEDIT_ALIAS;
      i--;
      break;
    case '3':
      OLC_MODE(d) = MEDIT_S_DESC;
      i--;
      break;
    case '4':
      OLC_MODE(d) = MEDIT_L_DESC;
      i--;
      break;
    case '5':
      OLC_MODE(d) = MEDIT_D_DESC;
      send_editor_help(d);
      write_to_output(d, "Enter mob description:\r\n\r\n");
      if (OLC_MOB(d)->player.description) {
	write_to_output(d, "%s", OLC_MOB(d)->player.description);
	oldtext = strdup(OLC_MOB(d)->player.description);
      }
      string_write(d, &OLC_MOB(d)->player.description, MAX_MOB_DESC, 0, oldtext);
      OLC_VAL(d) = 1;
      return;
    case '6':
      OLC_MODE(d) = MEDIT_LEVEL;
      i++;
      break;
    case '7':
      OLC_MODE(d) = MEDIT_ALIGNMENT;
      i++;
      break;
    case '8':
      OLC_MODE(d) = MEDIT_HITROLL;
      i++;
      break;
    case '9':
      OLC_MODE(d) = MEDIT_DAMROLL;
      i++;
      break;
    case 'a':
    case 'A':
      OLC_MODE(d) = MEDIT_NDD;
      i++;
      break;
    case 'b':
    case 'B':
      OLC_MODE(d) = MEDIT_SDD;
      i++;
      break;
    case 'c':
    case 'C':
      OLC_MODE(d) = MEDIT_NUM_HP_DICE;
      i++;
      break;
    case 'd':
    case 'D':
      OLC_MODE(d) = MEDIT_SIZE_HP_DICE;
      i++;
      break;
    case 'e':
    case 'E':
      OLC_MODE(d) = MEDIT_ADD_HP;
      i++;
      break;
    case 'f':
    case 'F':
      OLC_MODE(d) = MEDIT_AC;
      i++;
      break;
    case 'g':
    case 'G':
      OLC_MODE(d) = MEDIT_EXP;
      i++;
      break;
    case 'h':
    case 'H':
      OLC_MODE(d) = MEDIT_GOLD;
      i++;
      break;
    case 'i':
    case 'I':
      OLC_MODE(d) = MEDIT_POS;
      medit_disp_positions(d);
      return;
    case 'j':
    case 'J':
      OLC_MODE(d) = MEDIT_DEFAULT_POS;
      medit_disp_positions(d);
      return;
    case 'k':
    case 'K':
      OLC_MODE(d) = MEDIT_ATTACK;
      medit_disp_attack_types(d);
      return;
    case 'l':
    case 'L':
      OLC_MODE(d) = MEDIT_NPC_FLAGS;
      medit_disp_mob_flags(d);
      return;
    case 'm':
    case 'M':
      OLC_MODE(d) = MEDIT_AFF_FLAGS;
      medit_disp_aff_flags(d);
      return;
    case 'x': 
    case 'X': 
      write_to_output(d, "Are you sure you want to delete this mobile? "); 
      OLC_MODE(d) = MEDIT_DELETE; 
      return; 
#if CONFIG_OASIS_MPROG
    case 'p':
    case 'P':
      OLC_MODE(d) = MEDIT_MPROG;
      medit_disp_mprog(d);
      return;
#endif
    case 's':
    case 'S':
      OLC_SCRIPT_EDIT_MODE(d) = SCRIPT_MAIN_MENU;
      dg_script_menu(d);
      return;
    default:
      medit_disp_menu(d);
      return;
    }
    if (i == 0)
      break;
    else if (i == 1)
      write_to_output(d, "\r\nEnter new value : ");
    else if (i == -1)
      write_to_output(d, "\r\nEnter new text :\r\n] ");
    else
      write_to_output(d, "Oops...\r\n");
    return;
/*-------------------------------------------------------------------*/
  case OLC_SCRIPT_EDIT:
    if (dg_script_edit_parse(d, arg)) return;
    break;
/*-------------------------------------------------------------------*/
  case MEDIT_ALIAS:
    smash_tilde(arg);
    if (GET_ALIAS(OLC_MOB(d)))
      free(GET_ALIAS(OLC_MOB(d)));
    GET_ALIAS(OLC_MOB(d)) = str_udup(arg);
    break;
/*-------------------------------------------------------------------*/
  case MEDIT_S_DESC:
    smash_tilde(arg);
    if (GET_SDESC(OLC_MOB(d)))
      free(GET_SDESC(OLC_MOB(d)));
    GET_SDESC(OLC_MOB(d)) = str_udup(arg);
    break;
/*-------------------------------------------------------------------*/
  case MEDIT_L_DESC:
    smash_tilde(arg);
    if (GET_LDESC(OLC_MOB(d)))
      free(GET_LDESC(OLC_MOB(d)));
    if (arg && *arg) {
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%s\r\n", arg);
      GET_LDESC(OLC_MOB(d)) = strdup(buf);
    } else
      GET_LDESC(OLC_MOB(d)) = strdup("undefined");

    break;
/*-------------------------------------------------------------------*/
  case MEDIT_D_DESC:
    /*
     * We should never get here.
     */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: medit_parse(): Reached D_DESC case!");
    write_to_output(d, "Oops...\r\n");
    break;
/*-------------------------------------------------------------------*/
#if CONFIG_OASIS_MPROG
  case MEDIT_MPROG_COMLIST:
    /*
     * We should never get here, but if we do, bail out.
     */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: medit_parse(): Reached MPROG_COMLIST case!");
    break;
#endif
/*-------------------------------------------------------------------*/
  case MEDIT_NPC_FLAGS:
    if ((i = atoi(arg)) <= 0)
      break;
    else if (i <= NUM_MOB_FLAGS)
      TOGGLE_BIT(MOB_FLAGS(OLC_MOB(d)), 1 << (i - 1));
    medit_disp_mob_flags(d);
    return;
/*-------------------------------------------------------------------*/
  case MEDIT_AFF_FLAGS:
    if ((i = atoi(arg)) <= 0)
      break;
    else if (i <= NUM_AFF_FLAGS)
      TOGGLE_BIT(AFF_FLAGS(OLC_MOB(d)), 1 << (i - 1));

    /* Remove unwanted bits right away. */
    REMOVE_BIT(AFF_FLAGS(OLC_MOB(d)), 
               AFF_CHARM | AFF_POISON | AFF_GROUP | AFF_SLEEP);
    medit_disp_aff_flags(d);
    return;
/*-------------------------------------------------------------------*/
#if CONFIG_OASIS_MPROG
  case MEDIT_MPROG:
    if ((i = atoi(arg)) == 0)
      medit_disp_menu(d);
    else if (i == OLC_MTOTAL(d)) {
      struct mob_prog_data *temp;
      CREATE(temp, struct mob_prog_data, 1);
      temp->next = OLC_MPROGL(d);
      temp->type = -1;
      temp->arglist = NULL;
      temp->comlist = NULL;
      OLC_MPROG(d) = temp;
      OLC_MPROGL(d) = temp;
      OLC_MODE(d) = MEDIT_CHANGE_MPROG;
      medit_change_mprog (d);
    } else if (i < OLC_MTOTAL(d)) {
      struct mob_prog_data *temp;
      int x = 1;
      for (temp = OLC_MPROGL(d); temp && x < i; temp = temp->next)
        x++;
      OLC_MPROG(d) = temp;
      OLC_MODE(d) = MEDIT_CHANGE_MPROG;
      medit_change_mprog (d);
    } else if (i == (OLC_MTOTAL(d) + 1)) {
      write_to_output(d, "Which mob prog do you want to purge? ");
      OLC_MODE(d) = MEDIT_PURGE_MPROG;
    } else
      medit_disp_menu(d);
    return;

  case MEDIT_PURGE_MPROG:
    if ((i = atoi(arg)) > 0 && i < OLC_MTOTAL(d)) {
      struct mob_prog_data *temp;
      int x = 1;

      for (temp = OLC_MPROGL(d); temp && x < i; temp = temp->next)
	x++;
      OLC_MPROG(d) = temp;
      REMOVE_FROM_LIST(OLC_MPROG(d), OLC_MPROGL(d), next);
      free(OLC_MPROG(d)->arglist);
      free(OLC_MPROG(d)->comlist);
      free(OLC_MPROG(d));
      OLC_MPROG(d) = NULL;
      OLC_VAL(d) = 1;
    }
    medit_disp_mprog(d);
    return;

  case MEDIT_CHANGE_MPROG:
    if ((i = atoi(arg)) == 1)
      medit_disp_mprog_types(d);
    else if (i == 2) {
      write_to_output(d, "Enter new arg list: ");
      OLC_MODE(d) = MEDIT_MPROG_ARGS;
    } else if (i == 3) {
      write_to_output(d, "Enter new mob prog commands:\r\n");
      /*
       * Pass control to modify.c for typing.
       */
      OLC_MODE(d) = MEDIT_MPROG_COMLIST;
      if (OLC_MPROG(d)->comlist) {
        write_to_output(d, "%s", OLC_MPROG(d)->comlist);
        oldtext = strdup(OLC_MPROG(d)->comlist);
      }
      string_write(d, &OLC_MPROG(d)->comlist, MAX_STRING_LENGTH, 0, oldtext);
      OLC_VAL(d) = 1;
    } else
      medit_disp_mprog(d);
    return;
#endif

/*-------------------------------------------------------------------*/

/*
 * Numerical responses.
 */

#if CONFIG_OASIS_MPROG
  case MEDIT_MPROG_TYPE:
    /*
     * This calculation may be off by one too many powers of 2?
     * Someone who actually uses MobProgs will have to check.
     */
    OLC_MPROG(d)->type = (1 << LIMIT(atoi(arg), 0, NUM_PROGS - 1));
    OLC_VAL(d) = 1;
    medit_change_mprog(d);
    return;

  case MEDIT_MPROG_ARGS:
    OLC_MPROG(d)->arglist = strdup(arg);
    OLC_VAL(d) = 1;
    medit_change_mprog(d);
    return;
#endif

  case MEDIT_SEX:
    GET_SEX(OLC_MOB(d)) = LIMIT(i, 0, NUM_GENDERS - 1);
    break;

  case MEDIT_HITROLL:
    GET_HITROLL(OLC_MOB(d)) = LIMIT(i, 0, 50);
    break;

  case MEDIT_DAMROLL:
    GET_DAMROLL(OLC_MOB(d)) = LIMIT(i, 0, 50);
    break;

  case MEDIT_NDD:
    GET_NDD(OLC_MOB(d)) = LIMIT(i, 0, 30);
    break;

  case MEDIT_SDD:
    GET_SDD(OLC_MOB(d)) = LIMIT(i, 0, 127);
    break;

  case MEDIT_NUM_HP_DICE:
    GET_HIT(OLC_MOB(d)) = LIMIT(i, 0, 30);
    break;

  case MEDIT_SIZE_HP_DICE:
    GET_MANA(OLC_MOB(d)) = LIMIT(i, 0, 1000);
    break;

  case MEDIT_ADD_HP:
    GET_MOVE(OLC_MOB(d)) = LIMIT(i, 0, 30000);
    break;

  case MEDIT_AC:
    GET_AC(OLC_MOB(d)) = LIMIT(i, -200, 200);
    break;

  case MEDIT_EXP:
    GET_EXP(OLC_MOB(d)) = LIMIT(i, 0, MAX_MOB_EXP);
    break;

  case MEDIT_GOLD:
    GET_GOLD(OLC_MOB(d)) = LIMIT(i, 0, MAX_MOB_GOLD);
    break;

  case MEDIT_POS:
    GET_POS(OLC_MOB(d)) = LIMIT(i, 0, NUM_POSITIONS - 1);
    break;

  case MEDIT_DEFAULT_POS:
    GET_DEFAULT_POS(OLC_MOB(d)) = LIMIT(i, 0, NUM_POSITIONS - 1);
    break;

  case MEDIT_ATTACK:
    GET_ATTACK(OLC_MOB(d)) = LIMIT(i, 0, NUM_ATTACK_TYPES - 1);
    break;

  case MEDIT_LEVEL:
    GET_LEVEL(OLC_MOB(d)) = LIMIT(i, 1, 34);
    {
      int mob_lev = GET_LEVEL(OLC_MOB(d));

      GET_MOVE(OLC_MOB(d))    = mob_lev*10;          /* hit point bonus         */
      GET_HIT(OLC_MOB(d))     = mob_lev/5;           /* number of hitpoint dice */
      GET_MANA(OLC_MOB(d))    = mob_lev/5;           /* size of hitpoint dice   */
      GET_HITROLL(OLC_MOB(d)) = mob_lev/3;           /* hitroll 0-10            */
      GET_DAMROLL(OLC_MOB(d)) = mob_lev/6;           /* damroll 0-5             */
      GET_AC(OLC_MOB(d))      = (100-(mob_lev*6));   /* AC 94 to -80            */
      GET_NDD(OLC_MOB(d))     = MAX(1, mob_lev/6);   /* number damage dice 1-5  */
      GET_SDD(OLC_MOB(d))     = MAX(2, mob_lev/6);   /* size of damage dice 2-5 */
      GET_EXP(OLC_MOB(d))     = (mob_lev*mob_lev*100);
      GET_GOLD(OLC_MOB(d))    = (mob_lev*10);
    }
    break;

  case MEDIT_ALIGNMENT:
    GET_ALIGNMENT(OLC_MOB(d)) = LIMIT(i, -1000, 1000);
    break;

  case MEDIT_DELETE: 
    if (*arg == 'y' || *arg == 'Y') { 
      if (delete_mobile(GET_MOB_RNUM(OLC_MOB(d)))) 
        write_to_output(d, "Mobile deleted.\r\n"); 
      else 
        write_to_output(d, "Couldn't delete the mobile!\r\n"); 
 
      cleanup_olc(d, CLEANUP_ALL); 
      return; 
    } else if (*arg == 'n' || *arg == 'N') { 
      medit_disp_menu(d); 
      OLC_MODE(d) = MEDIT_MAIN_MENU; 
      return; 
    } else 
      write_to_output(d, "Please answer 'Y' or 'N': "); 
	    break;

/*-------------------------------------------------------------------*/
  default:
    /*
     * We should never get here.
     */
    cleanup_olc(d, CLEANUP_ALL);
    mudlog(BRF, LVL_BUILDER, TRUE, "SYSERR: OLC: medit_parse(): Reached default case!");
    write_to_output(d, "Oops...\r\n");
    break;
  }
/*-------------------------------------------------------------------*/

/*
 * END OF CASE 
 * If we get here, we have probably changed something, and now want to
 * return to main menu.  Use OLC_VAL as a 'has changed' flag  
 */

  OLC_VAL(d) = TRUE;
  medit_disp_menu(d);
}

void medit_string_cleanup(struct descriptor_data *d, int terminator)
{
  switch (OLC_MODE(d)) {

#if CONFIG_OASIS_MPROG
  case MEDIT_MPROG_COMLIST:
    medit_change_mprog(d);
    break;
#endif

  case MEDIT_D_DESC:
  default:
     medit_disp_menu(d);
     break;
  }
}

