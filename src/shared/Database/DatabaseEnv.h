/*
 * Copyright (C) 2013  BlizzLikeGroup
 * BlizzLikeCore integrates as part of this file: CREDITS.md and LICENSE.md
 */

#if !defined(DATABASEENV_H)
#define DATABASEENV_H

#include "Common.h"
#include "Log.h"
#include "Errors.h"

#include "Database/Field.h"
#include "Database/QueryResult.h"

#include "Database/Database.h"
typedef Database DatabaseType;
#define _LIKE_           "LIKE"
#define _TABLE_SIM_      "`"
#define _CONCAT3_(A,B,C) "CONCAT( " A " , " B " , " C " )"
#define _OFFSET_         "LIMIT %d,1"

extern DatabaseType WorldDatabase;
extern DatabaseType CharacterDatabase;
extern DatabaseType LoginDatabase;

#endif

