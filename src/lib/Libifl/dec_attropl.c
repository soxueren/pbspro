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


#include <pbs_config.h>   /* the master config generated by configure */

#include <stdlib.h>
#include "libpbs.h"
#include "dis.h"

/**
 * @file	dec_attropl.c
 */
/**
 * @brief
 *	decode into a list of PBS API "attropl" structures
 *
 *	The space for the attropl structures is allocated as needed.
 *
 *	The first item is a unsigned integer, a count of the
 *	number of attropl entries in the linked list.  This is encoded
 *	even when there are no entries in the list.
 *
 *	Each individual entry is encoded as:
 *		u int	size of the three strings (name, resource, value)
 *			including the terminating nulls, see dec_svrattrl.c
 *		string	attribute name
 *		u int	1 or 0 if resource name does or does not follow
 *		string	resource name (if one)
 *		string  value of attribute/resource
 *		u int	"op" of attrlop (also flag of svrattrl)
 *
 *	Note, the encoding of a attropl is the same as the encoding of
 *	the pbs_ifl.h structures "attrl" and the server struct svrattrl.
 *	Any one of the three forms can be decoded into any of the three with
 *	the possible loss of the "flags" field (which is the "op" of the
 *	attrlop).
 *
 * @param[in]   sock - socket descriptor
 * @param[in]   ppatt - pointer to list of attributes
 *
 * @return int
 * @retval 0 on SUCCESS
 * @retval >0 on failure
 */

int
decode_DIS_attropl(int sock, struct attropl **ppatt)
{
	int		 hasresc;
	int		 i;
	unsigned int	 numpat;
	struct attropl  *pat      = 0;
	struct attropl  *patprior = 0;
	int		 rc;


	numpat = disrui(sock, &rc);
	if (rc) return rc;

	for (i=0; i < numpat; ++i) {

		(void) disrui(sock, &rc);
		if (rc) break;

		pat = malloc(sizeof(struct attropl));
		if (pat == 0)
			return DIS_NOMALLOC;

		pat->next     = NULL;
		pat->name     = NULL;
		pat->resource = NULL;
		pat->value    = NULL;

		pat->name = disrst(sock, &rc);
		if (rc)	break;

		hasresc = disrui(sock, &rc);
		if (rc) break;
		if (hasresc) {
			pat->resource = disrst(sock, &rc);
			if (rc) break;
		}

		pat->value = disrst(sock, &rc);
		if (rc) break;

		pat->op = (enum batch_op)disrui(sock, &rc);
		if (rc) break;

		if (i == 0) {
			/* first one, link to passing in pointer */
			*ppatt = pat;
		} else {
			patprior->next = pat;
		}
		patprior = pat;
	}

	if (rc)
		PBS_free_aopl(pat);
	return rc;
}
