/** @file matchspy.cc
 * @brief MatchSpy implementation.
 */
/* Copyright (C) 2007,2008,2009 Olly Betts
 * Copyright (C) 2007,2009 Lemur Consulting Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <config.h>
#include <xapian/matchspy.h>

#include <xapian/document.h>
#include <xapian/error.h>
#include <xapian/queryparser.h>
#include <xapian/serialisationcontext.h>

#include <map>
#include <string>
#include <vector>

#include "autoptr.h"
#include "debuglog.h"
#include "omassert.h"
#include "serialise.h"
#include "stringutils.h"
#include "str.h"

#include <float.h>
#include <math.h>


using namespace std;

namespace Xapian {

MatchSpy::~MatchSpy() {}

MatchSpy *
MatchSpy::clone() const {
    throw UnimplementedError("MatchSpy not suitable for use with remote searches - clone() method unimplemented");
}

string
MatchSpy::name() const {
    throw UnimplementedError("MatchSpy not suitable for use with remote searches - name() method unimplemented");
}

string
MatchSpy::serialise() const {
    throw UnimplementedError("MatchSpy not suitable for use with remote searches - serialise() method unimplemented");
}

MatchSpy *
MatchSpy::unserialise(const string &, const SerialisationContext &) const {
    throw UnimplementedError("MatchSpy not suitable for use with remote searches - unserialise() method unimplemented");
}

string
MatchSpy::serialise_results() const {
    throw UnimplementedError("MatchSpy not suitable for use with remote searches - serialise_results() method unimplemented");
}

void
MatchSpy::merge_results(const string &) const {
    throw UnimplementedError("MatchSpy not suitable for use with remote searches - merge_results() method unimplemented");
}

string
MatchSpy::get_description() const {
    return "Xapian::MatchSpy()";
}


MultipleMatchSpy::~MultipleMatchSpy() {
    vector<MatchSpy *>::const_iterator i;
    for (i = owned_spies.begin(); i != owned_spies.end(); i++) {
	delete *i;
    }
}

void 
MultipleMatchSpy::operator()(const Document &doc, weight wt) {
    LOGCALL_VOID(MATCH, "MultipleMatchSpy::operator()", doc << ", " << wt);
    vector<MatchSpy *>::const_iterator i;
    for (i = spies.begin(); i != spies.end(); i++) {
	(**i)(doc, wt);
    }
}

MatchSpy *
MultipleMatchSpy::clone() const {
    AutoPtr<MultipleMatchSpy> res(new MultipleMatchSpy());
    vector<MatchSpy *>::const_iterator i;
    for (i = spies.begin(); i != spies.end(); ++i) {
	AutoPtr<MatchSpy> spy((*i)->clone());
	res->owned_spies.push_back(spy.get());
	res->append(spy.release());
    }
    return res.release();
}

string
MultipleMatchSpy::name() const {
    return "Xapian::MultipleMatchSpy";
}

string
MultipleMatchSpy::serialise() const {
    LOGCALL(REMOTE, string, "MultipleMatchSpy::serialise", "");
    string result;
    vector<MatchSpy *>::const_iterator i;
    for (i = spies.begin(); i != spies.end(); ++i) {
	string spy_name((*i)->name());
	result += encode_length(spy_name.size());
	result += spy_name;
	string serialised_spy((*i)->serialise());
	result += encode_length(serialised_spy.size());
	result += serialised_spy;
    }
    RETURN(result);
}

MatchSpy *
MultipleMatchSpy::unserialise(const string & s,
			      const SerialisationContext & context) const{
    LOGCALL(REMOTE, MatchSpy *, "MultipleMatchSpy::unserialise",
	    s << ", " << "context");
    const char * p = s.data();
    const char * end = p + s.size();

    AutoPtr<MultipleMatchSpy> res(new MultipleMatchSpy());
    while (p != end) {
	// First get the type of spy that we have.
	size_t len = decode_length(&p, end, true);
	string spy_name(p, len);
	p += len;
	const MatchSpy * spy_type = context.get_match_spy(spy_name);
	if (spy_type == NULL) {
	    throw NetworkError("Match spy type (" + spy_name + ") was not known by MultipleMatchSpy");
	}

	// Now, unserialise the spy appropriately.
	len = decode_length(&p, end, true);
	AutoPtr<MatchSpy> spy(spy_type->unserialise(string(p, len), context));
	p += len;
	res->owned_spies.push_back(spy.get());
	res->append(spy.release());
    }
    RETURN(res.release());
}

string
MultipleMatchSpy::serialise_results() const {
    LOGCALL(REMOTE, string, "MultipleMatchSpy::serialise_results", "");
    string result;
    vector<MatchSpy *>::const_iterator i;
    for (i = spies.begin(); i != spies.end(); ++i) {
	string subresults((*i)->serialise_results());
	result += encode_length(subresults.size());
	result += subresults;
    }
    RETURN(result);
}

void
MultipleMatchSpy::merge_results(const string & s) const {
    LOGCALL_VOID(REMOTE, "MultipleMatchSpy::merge_results", s);
    const char * p = s.data();
    const char * end = p + s.size();
    vector<MatchSpy *>::const_iterator i;
    for (i = spies.begin(); i != spies.end(); ++i) {
	size_t len = decode_length(&p, end, true);
	(*i)->merge_results(string(p, len));
	p += len;
    }
}

string
MultipleMatchSpy::get_description() const {
    string res("Xapian::MultipleMatchSpy(");
    vector<MatchSpy *>::const_iterator i;
    for (i = spies.begin(); i != spies.end(); ++i) {
	if (i != spies.begin()) {
	    res += ", ";
	}
	res += (*i)->get_description();
    }
    res += ")";
    return res;
}


void
StringListSerialiser::append(const string & value)
{
    serialised.append(encode_length(value.size()));
    serialised.append(value);
}

void
StringListUnserialiser::read_next()
{
    if (pos == NULL) {
	return;
    }
    if (pos == serialised.data() + serialised.size()) {
	pos = NULL;
	curritem.resize(0);
	return;
    }

    // FIXME - decode_length will throw a NetworkError if the length is too
    // long - should be a more appropriate error.
    size_t currlen = decode_length(&pos, serialised.data() + serialised.size(), true);
    curritem.assign(pos, currlen);
    pos += currlen;
}


/** Compare two StringAndFrequency objects.
 *
 *  The comparison is firstly by frequency (higher is better), then by string
 *  (earlier lexicographic sort is better).
 */
class StringAndFreqCmpByFreq {
  public:
    /// Default constructor
    StringAndFreqCmpByFreq() {}

    /// Return true if a has a higher frequency than b.
    /// If equal, compare by the str, to provide a stable sort order.
    bool operator()(const StringAndFrequency &a,
		    const StringAndFrequency &b) const {
	if (a.frequency > b.frequency) return true;
	if (a.frequency < b.frequency) return false;
	if (a.str > b.str) return false;
	return true;
    }
};

/** Get the most frequent items from a map from string to frequency.
 *
 *  This takes input such as that returned by @a
 *  ValueCountMatchSpy::get_values(), and returns a vector of the most
 *  frequent items in the input.
 *
 *  @param result A vector which will be filled with the most frequent
 *                items, in descending order of frequency.  Items with
 *                the same frequency will be sorted in ascending
 *                alphabetical order.
 *
 *  @param items The map from string to frequency, from which the most
 *               frequent items will be selected.
 *
 *  @param maxitems The maximum number of items to return.
 */
static void
get_most_frequent_items(vector<StringAndFrequency> & result,
			const map<string, doccount> & items,
			size_t maxitems)
{
    result.clear();
    result.reserve(maxitems);
    StringAndFreqCmpByFreq cmpfn;
    bool is_heap(false);

    for (map<string, doccount>::const_iterator i = items.begin();
	 i != items.end(); i++) {
	Assert(result.size() <= maxitems);
	result.push_back(StringAndFrequency(i->first, i->second));
	if (result.size() > maxitems) {
	    // Make the list back into a heap.
	    if (is_heap) {
		// Only the new element isn't in the right place.
		push_heap(result.begin(), result.end(), cmpfn);
	    } else {
		// Need to build heap from scratch.
		make_heap(result.begin(), result.end(), cmpfn);
		is_heap = true;
	    }
	    pop_heap(result.begin(), result.end(), cmpfn);
	    result.pop_back();
	}
    }

    if (is_heap) {
	sort_heap(result.begin(), result.end(), cmpfn);
    } else {
	sort(result.begin(), result.end(), cmpfn);
    }
}

void
ValueCountMatchSpy::operator()(const Document &doc, weight) {
    ++total;
    map<valueno, map<string, doccount> >::iterator i;
    for (i = values.begin(); i != values.end(); ++i) {
	valueno valno = i->first;
	map<string, doccount> & tally = i->second;

	if (multivalues.find(valno) != multivalues.end()) {
	    // Multiple values
	    StringListUnserialiser j(doc.get_value(valno));
	    StringListUnserialiser end;
	    for (; j != end; ++j) {
		string val(*j);
		if (!val.empty()) ++tally[val];
	    }
	} else {
	    // Single value
	    string val(doc.get_value(valno));
	    if (!val.empty()) ++tally[val];
	}
    }
}

void
ValueCountMatchSpy::get_top_values(vector<StringAndFrequency> & result,
				   valueno valno, size_t maxvalues) const
{
    get_most_frequent_items(result, get_values(valno), maxvalues);
}

MatchSpy *
ValueCountMatchSpy::clone() const {
    AutoPtr<ValueCountMatchSpy> res(new ValueCountMatchSpy());
    map<valueno, map<string, doccount> >::const_iterator i;
    for (i = values.begin(); i != values.end(); ++i) {
	set<valueno>::const_iterator j = multivalues.find(i->first);
	res->add_slot(i->first, j != multivalues.end());
    }
    return res.release();
}

string
ValueCountMatchSpy::name() const {
    return "Xapian::ValueCountMatchSpy";
}

string
ValueCountMatchSpy::serialise() const {
    string result;
    map<valueno, map<string, doccount> >::const_iterator i;
    for (i = values.begin(); i != values.end(); ++i) {
	result.append(encode_length(i->first));
	set<valueno>::const_iterator j = multivalues.find(i->first);
	if (j == multivalues.end()) {
	    result += '0';
	} else {
	    result += '1';
	}
    }
    return result;
}

MatchSpy *
ValueCountMatchSpy::unserialise(const string & s,
				const SerialisationContext &) const{
    const char * p = s.data();
    const char * end = p + s.size();

    AutoPtr<ValueCountMatchSpy> res(new ValueCountMatchSpy());
    while (p != end) {
	valueno new_slot = decode_length(&p, end, false);
	if (p == end || (p[0] != '0' && p[0] != '1')) {
	    throw NetworkError("Expected '0' or '1' to indicate multivalues status");
	}
	res->add_slot(new_slot, p[0] == '1');
	++p;
    }
    return res.release();
}

string
ValueCountMatchSpy::serialise_results() const {
    LOGCALL(REMOTE, string, "ValueCountMatchSpy::serialise_results", "");
    string result;
    result.append(encode_length(total));
    map<valueno, map<string, doccount> >::const_iterator i;
    for (i = values.begin(); i != values.end(); ++i) {
	result.append(encode_length(i->first));
	result.append(encode_length(i->second.size()));
	map<string, doccount>::const_iterator j;
	for (j = i->second.begin(); j != i->second.end(); ++j) {
	    result.append(encode_length(j->first.size()));
	    result.append(j->first);
	    result.append(encode_length(j->second));
	}
    }
    RETURN(result);
}

void
ValueCountMatchSpy::merge_results(const string & s) const {
    LOGCALL_VOID(REMOTE, "ValueCountMatchSpy::merge_results", s);
    const char * p = s.data();
    const char * end = p + s.size();

    total += decode_length(&p, end, false);
    while (p != end) {
	valueno new_slot = decode_length(&p, end, false);
	map<string, doccount>::size_type items;
	items = decode_length(&p, end, false);
	while(items != 0) {
	    map<string, doccount> & tally = values[new_slot];
	    size_t vallen = decode_length(&p, end, true);
	    string val(p, vallen);
	    p += vallen;
	    doccount freq = decode_length(&p, end, false);
	    tally[val] += freq;
	    --items;
	}
    }
}

string
ValueCountMatchSpy::get_description() const {
    return "Xapian::ValueCountMatchSpy(" + str(total) +
	    " docs seen, looking in " + str(values.size()) + " slots)";
}


void
TermCountMatchSpy::operator()(const Document &doc, weight) {
    ++documents_seen;
    map<string, map<string, doccount> >::iterator i;
    for (i = terms.begin(); i != terms.end(); ++i) {
	string prefix = i->first;
	map<string, doccount> & tally = i->second;

	TermIterator j = doc.termlist_begin();
	j.skip_to(prefix);
	for (; j != doc.termlist_end() && startswith((*j), prefix); ++j) {
	    if ((*j).size() <= prefix.size())
		continue;
	    char firstchar = (*j)[prefix.size()];
	    if (firstchar >= 'A' && firstchar <= 'Z')
		continue;
	    ++tally[(*j).substr(prefix.size())];
	    ++terms_seen;
	}
    }
}

void
TermCountMatchSpy::get_top_terms(vector<StringAndFrequency> & result,
				 string prefix, size_t maxterms) const
{
    get_most_frequent_items(result, get_terms(prefix), maxterms);
}

MatchSpy *
TermCountMatchSpy::clone() const {
    AutoPtr<TermCountMatchSpy> res(new TermCountMatchSpy());
    map<string, map<string, doccount> >::const_iterator i;
    for (i = terms.begin(); i != terms.end(); ++i) {
	res->add_prefix(i->first);
    }
    return res.release();
}

string
TermCountMatchSpy::name() const {
    return "Xapian::TermCountMatchSpy";
}

string
TermCountMatchSpy::serialise() const {
    string result;
    map<string, map<string, doccount> >::const_iterator i;
    for (i = terms.begin(); i != terms.end(); ++i) {
	result += encode_length(i->first.size());
	result += i->first;
    }
    return result;
}

MatchSpy *
TermCountMatchSpy::unserialise(const string & s,
			       const SerialisationContext &) const{
    const char * p = s.data();
    const char * end = p + s.size();

    AutoPtr<TermCountMatchSpy> res(new TermCountMatchSpy());
    while (p != end) {
	size_t len = decode_length(&p, end, true);
	res->add_prefix(string(p, len));
	p += len;
    }
    return res.release();
}

string
TermCountMatchSpy::serialise_results() const {
    LOGCALL(REMOTE, string, "TermCountMatchSpy::serialise_results", "");
    string result;
    result.append(encode_length(documents_seen));
    result.append(encode_length(terms_seen));
    map<string, map<string, doccount> >::const_iterator i;
    for (i = terms.begin(); i != terms.end(); ++i) {
	result += encode_length(i->first.size());
	result += i->first;
	result += encode_length(i->second.size());
	map<string, doccount>::const_iterator j;
	for (j = i->second.begin(); j != i->second.end(); ++j) {
	    result.append(encode_length(j->first.size()));
	    result.append(j->first);
	    result.append(encode_length(j->second));
	}
    }
    RETURN(result);
}

void
TermCountMatchSpy::merge_results(const string & s) const {
    LOGCALL_VOID(REMOTE, "TermCountMatchSpy::merge_results", s);
    const char * p = s.data();
    const char * end = p + s.size();

    documents_seen += decode_length(&p, end, false);
    terms_seen += decode_length(&p, end, false);
    while (p != end) {
	size_t len = decode_length(&p, end, true);
	string term(p, len);
	p += len;
	map<string, doccount>::size_type items;
	items = decode_length(&p, end, false);
	while(items != 0) {
	    map<string, doccount> & tally = terms[term];
	    size_t vallen = decode_length(&p, end, true);
	    string val(p, vallen);
	    p += vallen;
	    doccount freq = decode_length(&p, end, false);
	    tally[val] += freq;
	    --items;
	}
    }
}

string
TermCountMatchSpy::get_description() const {
    return "Xapian::TermCountMatchSpy(" + str(documents_seen) +
	    " docs seen, " + str(terms_seen) +
	    " terms_seen, looking in " + str(terms.size()) + " slots)";
}


inline double sqrd(double x) { return x * x; }

double
CategorySelectMatchSpy::score_categorisation(valueno valno,
					     double desired_no_of_categories)
{
    if (total == 0) return 0.0;

    const map<string, doccount> & cat = values[valno];
    size_t total_unset = total;
    double score = 0.0;

    if (desired_no_of_categories <= 0.0)
	desired_no_of_categories = cat.size();

    double avg = double(total) / desired_no_of_categories;

    map<string, doccount>::const_iterator i;
    for (i = cat.begin(); i != cat.end(); ++i) {
	size_t count = i->second;
	total_unset -= count;
	score += sqrd(count - avg);
    }
    if (total_unset) score += sqrd(total_unset - avg);

    // Scale down so the total number of items doesn't make a difference.
    score /= sqrd(total);

    // Bias towards returning the number of categories requested.
    score += 0.01 * sqrd(desired_no_of_categories - cat.size());

    return score;
}

struct bucketval {
    size_t count;
    double min, max;

    bucketval() : count(0), min(DBL_MAX), max(-DBL_MAX) { }

    void update(size_t n, double value) {
	count += n;
	if (value < min) min = value;
	if (value > max) max = value;
    }
};

bool
CategorySelectMatchSpy::build_numeric_ranges(valueno valno, size_t max_ranges)
{
    const map<string, doccount> & cat = values[valno];

    double lo = DBL_MAX, hi = -DBL_MAX;

    map<double, doccount> histo;
    doccount total_set = 0;
    map<string, doccount>::const_iterator i;
    for (i = cat.begin(); i != cat.end(); ++i) {
	if (i->first.size() == 0) continue;
	double v = sortable_unserialise(i->first.c_str());
	if (v < lo) lo = v;
	if (v > hi) hi = v;
	doccount count = i->second;
	histo[v] = count;
	total_set += count;
    }

    if (total_set == 0) {
	// No set values.
	return false;
    }
    if (lo == hi) {
	// All set values are the same.
	return false;
    }

    double sizeby = max(fabs(hi), fabs(lo));
    // E.g. if sizeby = 27.4 and max_ranges = 7, we want to split into units of
    // width 1.0 which we may then coalesce if there are too many used buckets.
    double unit = pow(10.0, floor(log10(sizeby / max_ranges) - 0.2));
    double start = floor(lo / unit) * unit;
    // Can happen due to FP rounding (e.g. lo = 11.95, unit = 0.01).
    if (start > lo) start = lo;
    size_t n_buckets = size_t(ceil(hi / unit) - floor(lo / unit));

    bool scaleby2 = true;
    vector<bucketval> bucket(n_buckets + 1);
    while (true) {
	size_t n_used = 0;
	map<double, doccount>::const_iterator j;
	for (j = histo.begin(); j != histo.end(); ++j) {
	    double v = j->first;
	    size_t b = size_t(floor((v - start) / unit));
	    if (b > n_buckets) b = n_buckets; // FIXME - Hacky workaround to ensure that b is in range.
	    if (bucket[b].count == 0) ++n_used;
	    bucket[b].update(j->second, v);
	}

	if (n_used <= max_ranges) break;

	unit *= scaleby2 ? 2.0 : 2.5;
	scaleby2 = !scaleby2;
	start = floor(lo / unit) * unit;
	// Can happen due to FP rounding (e.g. lo = 11.95, unit = 0.01).
	if (start > lo) start = lo;
	n_buckets = size_t(ceil(hi / unit) - floor(lo / unit));
	bucket.resize(0);
	bucket.resize(n_buckets + 1);
    }

    map<string, doccount> discrete_categories;
    for (size_t b = 0; b < bucket.size(); ++b) {
	if (bucket[b].count == 0) continue;
	string encoding = sortable_serialise(bucket[b].min);
	if (bucket[b].min != bucket[b].max) {
	    // Pad the start to 9 bytes with zeros.
	    encoding.resize(9);
	    encoding += sortable_serialise(bucket[b].max);
	}
	discrete_categories[encoding] = bucket[b].count;
    }

    size_t total_unset = total - total_set;
    if (total_unset) {
	discrete_categories[""] = total_unset;
    }

    swap(discrete_categories, values[valno]);

    return true;
}

}