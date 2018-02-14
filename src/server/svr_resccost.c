/*
 * Copyright (C) 1994-2018 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *
 * This file is part of the PBS Professional ("PBS Pro") software.
 *
 * Open Source License Information:
 *
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Commercial License Information:
 *
 * For a copy of the commercial license terms and conditions,
 * go to: (http://www.pbspro.com/UserArea/agreement.html)
 * or contact the Altair Legal Department.
 *
 * Altair’s dual-license business model allows companies, individuals, and
 * organizations to create proprietary derivative works of PBS Pro and
 * distribute them - whether embedded or bundled with other software -
 * under a commercial license agreement.
 *
 * Use of Altair’s trademarks, including but not limited to "PBS™",
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
 * trademark licensing policies.
 *
 */

/**
 * @file    svr_resccost.c
 *
 * @brief
 * 		svr_resccost.c - This file contains the functions for manipulating the server
 * 		attribute "resource cost", which is of type ATR_TYPE_LIST
 *
 *  	It contains functions for:
 *		Decoding the value string to the machine representation,
 *		a long integer within the resource cost structure.
 *		Encoding the long integer value to external form
 *		Setting the value by =, + or - operators.
 *		Freeing the storage space used by the list.
 *
 *		note - it was my original intent to have the cost be an integer recorded
 *		in the resource_defination structure itself.  It seemed logical, one
 *		value per definition, why not.  But "the old atomic set" destroys that
 *		idea.  Have to be able to have temporary attributes with their own
 *		values...  Hence it came down to another linked-list of values.
 *
 *		Resource_cost entry, one per resource type which has been set.
 * 		The list is headed in the resource_cost attribute.
 *
 * Included functions are:
 *	decode_rcost()
 *	encode_rcost()
 *	set_rcost()
 *	free_rcost()
 *
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_error.h"
#include "server_limits.h"
#include "server.h"
#include "job.h"



struct resource_cost {
	pbs_list_link	rc_link;
	resource_def   *rc_def;
	long		rc_cost;
};

/**
 * @brief
 * 		add_cost_entry	-	add a new cost entry to the resource_cost list.
 *
 * @param[in,out]	patr	-	attribute structure
 * @param[in]	prdef	-	resource definition structure
 *
 * @return	resource_cost *
 */

static struct resource_cost *add_cost_entry(attribute *patr, resource_def *prdef)
{
	struct resource_cost *pcost;

	pcost = malloc(sizeof(struct resource_cost));
	if (pcost) {
		CLEAR_LINK(pcost->rc_link);
		pcost->rc_def = prdef;
		pcost->rc_cost = 0;
		append_link(&patr->at_val.at_list, &pcost->rc_link, pcost);
	}
	return (pcost);
}

/**
 * @brief
 * 		decode_rcost - decode string into resource cost value
 *
 * @param[in,out]	patr	-	attribute name
 * @param[in]	name	-	attribute name
 * @param[in]	rescn	-	resource name, unused here
 * @param[in]	val	-	attribute value
 *
 * @return	int
 * @retval	0	: if ok
 * @retval	>0	: error number if error
 * @retval	*patr
 *	Returns: 0 if ok
 *		>0 error number if error
 */

int
decode_rcost(struct attribute *patr, char *name, char *rescn, char *val)
{
	resource_def *prdef;
	struct resource_cost *pcost;
	void free_rcost(attribute *);


	if ((val == NULL) || (rescn == NULL)) {
		patr->at_flags = (patr->at_flags & ~ATR_VFLAG_SET) |
			ATR_VFLAG_MODIFY;
		return (0);
	}
	if (patr->at_flags & ATR_VFLAG_SET) {
		free_rcost(patr);
	}

	prdef = find_resc_def(svr_resc_def, rescn, svr_resc_size);
	if (prdef == NULL)
		return (PBSE_UNKRESC);
	pcost = (struct resource_cost *)GET_NEXT(patr->at_val.at_list);
	while (pcost) {
		if (pcost->rc_def == prdef)
			break;	/* have entry in attr already */
		pcost = (struct resource_cost *)GET_NEXT(pcost->rc_link);
	}
	if (pcost == NULL) {	/* add entry */
		if ((pcost=add_cost_entry(patr, prdef)) == NULL)
			return (PBSE_SYSTEM);
	}
	pcost->rc_cost = atol(val);
	patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;
	return (0);
}

/**
 * @brief
 * 		encode_rcost - encode attribute of type long into attr_extern
 *
 * @param[in]	attr	-	ptr to attribute
 * @param[in,out]	phead	-	head of attrlist list
 * @param[in]	atname	-	attribute name
 * @param[in]	rsname	-	esource name or null
 * @param[in]	mode	-	encode mode, unused here
 * @param[out]	rtnl	-	RETURN: ptr to svrattrl
 *
 * @return	int
 * @retval	>0	: if ok
 * @retval	=0	: if no value, no attrlist link added
 * @retval	<0	: if error
 */
/*ARGSUSED*/


int
encode_rcost(attribute *attr, pbs_list_head *phead, char *atname, char *rsname, int mode, svrattrl **rtnl)
{
	svrattrl *pal;
	struct resource_cost *pcost;
	int	  first = 1;
	svrattrl *xprior = NULL;

	if (!attr)
		return (-1);
	if (!(attr->at_flags & ATR_VFLAG_SET))
		return (0);

	pcost = (struct resource_cost *)GET_NEXT(attr->at_val.at_list);
	while (pcost) {
		rsname = pcost->rc_def->rs_name;
		if ((pal = attrlist_create(atname, rsname, 23)) == NULL)
			return (-1);

		(void)sprintf(pal->al_value, "%ld", pcost->rc_cost);
		pal->al_flags = attr->at_flags;
		append_link(phead, &pal->al_link, pal);
		if (first) {
			if (rtnl)
				*rtnl  = pal;
			first  = 0;
		} else {
			xprior->al_sister = pal;
		}
		xprior = pal;

		pcost = (struct resource_cost *)GET_NEXT(pcost->rc_link);
	}

	return (1);
}

/**
 * @brief
 * 		set_rcost - set attribute A to attribute B,
 *		either A=B, A += B, or A -= B
 *
 * @param[in,out]	old	-	attribute A
 * @param[in]	new	-	attribute B
 * @param[in]	op	-	batch operator. Ex: SET, INCR, DECR.
 *
 * @return	int
 * @retval	0	: if ok
 * @retval	>0 	: if error
 */

int
set_rcost(struct attribute *old, struct attribute *new, enum batch_op op)
{
	struct resource_cost *pcnew;
	struct resource_cost *pcold;

	assert(old && new && (new->at_flags & ATR_VFLAG_SET));

	pcnew = (struct resource_cost *)GET_NEXT(new->at_val.at_list);
	while (pcnew) {
		pcold = (struct resource_cost *)GET_NEXT(old->at_val.at_list);
		while (pcold) {
			if (pcnew->rc_def == pcold->rc_def)
				break;
			pcold = (struct resource_cost *)GET_NEXT(pcold->rc_link);
		}
		if (pcold == NULL)
			if ((pcold = add_cost_entry(old, pcnew->rc_def)) == NULL)
				return (PBSE_SYSTEM);

		switch (op) {
			case SET:   pcold->rc_cost = pcnew->rc_cost;
				break;

			case INCR:  pcold->rc_cost += pcnew->rc_cost;
				break;

			case DECR:  pcold->rc_cost -= pcnew->rc_cost;
				break;

			default:    return (PBSE_INTERNAL);
		}
		pcnew = (struct resource_cost *)GET_NEXT(pcnew->rc_link);
	}
	old->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_MODCACHE;
	return (0);
}


/**
 * @brief
 * 		free_rcost - free space used by resource cost attribute
 *
 * @param[in]	pattr	-	attribute structure
 */

void
free_rcost(attribute *pattr)
{
	struct resource_cost *pcost;

	while ((pcost = (struct resource_cost *)GET_NEXT(
		pattr->at_val.at_list)) != NULL) {
		delete_link(&pcost->rc_link);
		(void)free(pcost);
	}
	pattr->at_flags &= ~ATR_VFLAG_SET;
}

