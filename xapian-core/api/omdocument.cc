/* omdocument.cc: class for performing a match
 *
 * ----START-LICENCE----
 * Copyright 1999,2000,2001 BrightStation PLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 * -----END-LICENCE-----
 */

#include "om/omdocument.h"
#include "om/omtypes.h"
#include "refcnt.h"
#include "document.h"
#include "omdocumentinternal.h"
#include "omtermlistiteratorinternal.h"
#include "omkeylistiteratorinternal.h"
#include "om/omerror.h"
#include "omdatabaseinterface.h"
#include "omdatabaseinternal.h"
#include <algorithm>

//////////////////////////////////
// implementation of OmDocument //
//////////////////////////////////

OmDocument::OmDocument(OmDocument::Internal *internal_) : internal(internal_)
{
}

OmDocument::OmDocument() : internal(new OmDocument::Internal)
{
}

OmKey
OmDocument::get_key(om_keyno key) const
{
    DEBUGAPICALL(OmKey, "OmDocument::get_key", key);
    if (internal->keys_here) {
	std::map<om_keyno, OmKey>::const_iterator i;
	i = internal->keys.find(key);
	if (i == internal->keys.end()) {
	    OmKey nul;
	    RETURN(nul);
	}
	RETURN(i->second);
    }
    // create our own RefCntPtr in case another thread assigns a new ptr
    RefCntPtr<Document> myptr = internal->ptr;

    RETURN(myptr->get_key(key));
}

OmData
OmDocument::get_data() const
{
    DEBUGAPICALL(OmData, "OmDocument::get_data", "");
    if (internal->data_here) RETURN(internal->data);
    // create our own RefCntPtr in case another thread assigns a new ptr
    RefCntPtr<Document> myptr = internal->ptr;

    RETURN(myptr->get_data());
}

void
OmDocument::set_data(const OmData &data)
{
    DEBUGAPICALL(void, "OmDocument::set_data", data);
    // FIXME: locking???
    internal->data = data;
    internal->data_here = true;
}

void
OmDocument::set_data(const std::string &data)
{
    set_data(OmData(data));
}

void
OmDocument::operator=(const OmDocument &other)
{
    // pointers are reference counted.
    *internal = *(other.internal);
}

OmDocument::OmDocument(const OmDocument &other)
	: internal(0)
{
    internal = new Internal(*other.internal);
}

OmDocument::~OmDocument()
{
    delete internal;
}

std::string
OmDocument::get_description() const
{
    std::string description = "OmDocument(data=";
    if (internal->data_here)  description += "`" + internal->data.value + "'";
    else                      description += "[not fetched]";

    description += " keys=";
    if (internal->keys_here)  description += om_tostring(internal->keys.size());
    else                      description += "[not fetched]";

    description += " terms=";
    if (internal->terms_here) description += om_tostring(internal->terms.size());
    else                      description += "[not fetched]";

    return description + ")";
}

void
OmDocument::add_key(om_keyno keyno, const OmKey &key)
{
    DEBUGAPICALL(void, "OmDocument::add_key", keyno << ", " << key);
    // FIXME: need to lock here...
    if (!internal->keys_here) {
	internal->keys = internal->ptr->get_all_keys();
	internal->keys_here = true;
    }
    internal->keys.insert(std::make_pair(keyno, key));	
}

void
OmDocument::remove_key(om_keyno keyno)
{
    DEBUGAPICALL(void, "OmDocument::remove_key", keyno);
    // FIXME: need to lock here...
    if (!internal->keys_here) {
	internal->keys = internal->ptr->get_all_keys();
	internal->keys_here = true;
    }
    internal->keys.erase(keyno);
}

void
OmDocument::clear_keys()
{
    DEBUGAPICALL(void, "OmDocument::clear_keys", "");
    internal->keys.clear();
    internal->keys_here = true;
}

void
OmDocument::Internal::read_termlist(OmTermIterator t,
				    const OmTermIterator & tend)
{
    // FIXME: need to lock here...
    if (!terms_here) {
	for ( ; t != tend; t++) {
	    OmPositionListIterator p = t.positionlist_begin();
	    OmPositionListIterator pend = t.positionlist_end();
	    OmDocumentTerm term(*t);
	    for ( ; p != pend; p++) {
		term.add_position(*p);
	    }
	    term.set_wdf(t.get_wdf());
	    terms.insert(std::make_pair(*t, term));
	}
	terms_here = true;
    }
}

void
OmDocument::add_posting(const om_termname & tname,
			om_termpos tpos,
			om_termcount wdfinc)
{
    DEBUGAPICALL(void, "OmDocument::add_posting",
		 tname << ", " << tpos << ", " << wdfinc);
    if (tname.empty()) {
	throw OmInvalidArgumentError("Empty termnames aren't allowed.");
    }
    internal->read_termlist(termlist_begin(), termlist_end());

    std::map<om_termname, OmDocumentTerm>::iterator i;
    i = internal->terms.find(tname);
    if (i == internal->terms.end()) {
	OmDocumentTerm newterm(tname);
	newterm.add_position(tpos);
	newterm.set_wdf(wdfinc);
	internal->terms.insert(std::make_pair(tname, newterm));
    } else {
	i->second.add_position(tpos);
	if (wdfinc) i->second.set_wdf(i->second.get_wdf() + wdfinc);
    }
}

void
OmDocument::add_term_nopos(const om_termname & tname,
			   om_termcount wdfinc)
{
    DEBUGAPICALL(void, "OmDocument::add_term_nopos", tname << ", " << wdfinc);
    if (tname.empty()) {
	throw OmInvalidArgumentError("Empty termnames aren't allowed.");
    }
    internal->read_termlist(termlist_begin(), termlist_end());

    std::map<om_termname, OmDocumentTerm>::iterator i;
    i = internal->terms.find(tname);
    if (i == internal->terms.end()) {
	OmDocumentTerm newterm(tname);
	newterm.set_wdf(wdfinc);
	internal->terms.insert(std::make_pair(tname, newterm));
    } else {
	if (wdfinc) i->second.set_wdf(i->second.get_wdf() + wdfinc);
    }
}

void
OmDocument::remove_posting(const om_termname & tname,
			   om_termpos tpos,
			   om_termcount wdfdec)
{
    DEBUGAPICALL(void, "OmDocument::remove_posting",
		 tname << ", " << tpos << ", " << wdfdec);
    if (tname.empty()) {
	throw OmInvalidArgumentError("Empty termnames aren't allowed.");
    }
    internal->read_termlist(termlist_begin(), termlist_end());

    std::map<om_termname, OmDocumentTerm>::iterator i;
    i = internal->terms.find(tname);
    if (i == internal->terms.end()) {
	throw OmInvalidArgumentError("Term `" + tname +
				     "' is not present in document, in "
				     "OmDocument::remove_posting()");
    } else {
	i->second.remove_position(tpos);
	if (wdfdec) {
	    om_termcount currwdf = i->second.get_wdf();
	    currwdf = ((currwdf > wdfdec) ? (currwdf - wdfdec) : 0);
	    i->second.set_wdf(currwdf);
	}
    }
}

void
OmDocument::remove_term(const om_termname & tname)
{
    DEBUGAPICALL(void, "OmDocument::remove_term", tname);
    internal->read_termlist(termlist_begin(), termlist_end());
    std::map<om_termname, OmDocumentTerm>::iterator i;
    i = internal->terms.find(tname);
    if (i == internal->terms.end()) {
	throw OmInvalidArgumentError("Term `" + tname +
				     "' is not present in document, in "
				     "OmDocument::remove_term()");
    } else {
	internal->terms.erase(i);
    }
}


void
OmDocument::clear_terms()
{
    DEBUGAPICALL(void, "OmDocument::clear_terms", "");
    if (internal->terms_here) {
	internal->terms.clear();
    } else {
	internal->terms_here = true;
    }
}

OmTermIterator
OmDocument::termlist_begin() const
{
    DEBUGAPICALL(OmTermIterator, "OmDocument::termlist_begin", "");
    if (internal->terms_here) {
	RETURN(OmTermIterator(new OmTermIterator::Internal(
		new MapTermList(internal->terms.begin(),
				internal->terms.end(),
				internal->terms.size()))));
    }
    RETURN(OmTermIterator(new OmTermIterator::Internal(
		internal->ptr->open_term_list(),
		internal->database,
		internal->did)));
}

OmTermIterator
OmDocument::termlist_end() const
{
    DEBUGAPICALL(OmTermIterator, "OmDocument::termlist_end", "");
    RETURN(OmTermIterator(NULL));
}

OmKeyListIterator
OmDocument::keylist_begin() const
{
    DEBUGAPICALL(OmKeyListIterator, "OmDocument::keylist_begin", "");
    // FIXME: need to lock here...
    if (!internal->keys_here) {
	internal->keys = internal->ptr->get_all_keys();
	internal->keys_here = true;
    }
    RETURN(OmKeyListIterator(new OmKeyListIterator::Internal(internal->keys.begin())));
}

OmKeyListIterator
OmDocument::keylist_end() const
{
    DEBUGAPICALL(OmKeyListIterator, "OmDocument::keylist_end", "");
    // FIXME: need to lock here...
    if (!internal->keys_here) {
	internal->keys = internal->ptr->get_all_keys();
	internal->keys_here = true;
    }
    RETURN(OmKeyListIterator(new OmKeyListIterator::Internal(internal->keys.end())));
}

OmDocumentTerm::OmDocumentTerm(const om_termname & tname_)
	: tname(tname_),
	  wdf(0),
	  termfreq(0)
{
    DEBUGAPICALL(void, "OmDocumentTerm::OmDocumentTerm", tname_);
}

/////////////////////////////////////////////////////////////////////////////

void
OmDocumentTerm::add_position(om_termpos tpos)
{
    DEBUGAPICALL(void, "OmDocumentTerm::add_position", tpos);
    
    // We generally expect term positions to be added in approximately
    // increasing order, so check the end first
    om_termpos last = positions.empty() ? 0 : positions.back();
    if (tpos > last) {
	positions.push_back(tpos);
	return;
    }

    // Search for the position the term occurs at.  Use binary chop to
    // search, since this is a sorted list.
    std::vector<om_termpos>::iterator i;
    i = std::lower_bound(positions.begin(), positions.end(), tpos);
    if (i == positions.end() || *i != tpos) {
	positions.insert(i, tpos);
    }
}

void
OmDocumentTerm::remove_position(om_termpos tpos)
{
    DEBUGAPICALL(void, "OmDocumentTerm::remove_position", tpos);
    
    // Search for the position the term occurs at.  Use binary chop to
    // search, since this is a sorted list.
    std::vector<om_termpos>::iterator i;
    i = std::lower_bound(positions.begin(), positions.end(), tpos);
    if (i == positions.end() || *i != tpos) {
	throw OmInvalidArgumentError("Position `" + om_tostring(tpos) +
				     "' not found in list of positions that `" +
				     tname +
				     "' occurs at,"
				     " when removing position from list");
	positions.insert(i, tpos);
    } else {
	positions.erase(i);
    }
}

std::string
OmDocumentTerm::get_description() const
{
    DEBUGCALL(INTRO, std::string, "OmDocumentTerm::get_description", "");
    std::string description;

    description = "OmDocumentTerm(" + tname +
	    ", wdf = " + om_tostring(wdf) +
	    ", termfreq = " + om_tostring(termfreq) +
	    ", positions[" + om_tostring(positions.size()) + "]" +
	    ")";
    RETURN(description);
}
