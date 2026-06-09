//
// Generated file, do not edit! Created by opp_msgtool 6.3 from flood/FloodPacket.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include <type_traits>
#include "FloodPacket_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp

namespace flood {

Register_Class(FloodPacket)

FloodPacket::FloodPacket() : ::inet::FieldsChunk()
{
}

FloodPacket::FloodPacket(const FloodPacket& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

FloodPacket::~FloodPacket()
{
}

FloodPacket& FloodPacket::operator=(const FloodPacket& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void FloodPacket::copy(const FloodPacket& other)
{
    this->src = other.src;
    this->dst = other.dst;
    this->lastHop = other.lastHop;
    this->packetId = other.packetId;
    this->hopLimit = other.hopLimit;
    this->hopStart = other.hopStart;
    this->wantAck = other.wantAck;
    this->txTimeNs = other.txTimeNs;
    this->payload = other.payload;
}

void FloodPacket::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->src);
    doParsimPacking(b,this->dst);
    doParsimPacking(b,this->lastHop);
    doParsimPacking(b,this->packetId);
    doParsimPacking(b,this->hopLimit);
    doParsimPacking(b,this->hopStart);
    doParsimPacking(b,this->wantAck);
    doParsimPacking(b,this->txTimeNs);
    doParsimPacking(b,this->payload);
}

void FloodPacket::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->src);
    doParsimUnpacking(b,this->dst);
    doParsimUnpacking(b,this->lastHop);
    doParsimUnpacking(b,this->packetId);
    doParsimUnpacking(b,this->hopLimit);
    doParsimUnpacking(b,this->hopStart);
    doParsimUnpacking(b,this->wantAck);
    doParsimUnpacking(b,this->txTimeNs);
    doParsimUnpacking(b,this->payload);
}

uint32_t FloodPacket::getSrc() const
{
    return this->src;
}

void FloodPacket::setSrc(uint32_t src)
{
    handleChange();
    this->src = src;
}

uint32_t FloodPacket::getDst() const
{
    return this->dst;
}

void FloodPacket::setDst(uint32_t dst)
{
    handleChange();
    this->dst = dst;
}

uint32_t FloodPacket::getLastHop() const
{
    return this->lastHop;
}

void FloodPacket::setLastHop(uint32_t lastHop)
{
    handleChange();
    this->lastHop = lastHop;
}

uint32_t FloodPacket::getPacketId() const
{
    return this->packetId;
}

void FloodPacket::setPacketId(uint32_t packetId)
{
    handleChange();
    this->packetId = packetId;
}

uint8_t FloodPacket::getHopLimit() const
{
    return this->hopLimit;
}

void FloodPacket::setHopLimit(uint8_t hopLimit)
{
    handleChange();
    this->hopLimit = hopLimit;
}

uint8_t FloodPacket::getHopStart() const
{
    return this->hopStart;
}

void FloodPacket::setHopStart(uint8_t hopStart)
{
    handleChange();
    this->hopStart = hopStart;
}

bool FloodPacket::getWantAck() const
{
    return this->wantAck;
}

void FloodPacket::setWantAck(bool wantAck)
{
    handleChange();
    this->wantAck = wantAck;
}

uint64_t FloodPacket::getTxTimeNs() const
{
    return this->txTimeNs;
}

void FloodPacket::setTxTimeNs(uint64_t txTimeNs)
{
    handleChange();
    this->txTimeNs = txTimeNs;
}

const char * FloodPacket::getPayload() const
{
    return this->payload.c_str();
}

void FloodPacket::setPayload(const char * payload)
{
    handleChange();
    this->payload = payload;
}

class FloodPacketDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_src,
        FIELD_dst,
        FIELD_lastHop,
        FIELD_packetId,
        FIELD_hopLimit,
        FIELD_hopStart,
        FIELD_wantAck,
        FIELD_txTimeNs,
        FIELD_payload,
    };
  public:
    FloodPacketDescriptor();
    virtual ~FloodPacketDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(FloodPacketDescriptor)

FloodPacketDescriptor::FloodPacketDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(flood::FloodPacket)), "inet::FieldsChunk")
{
    propertyNames = nullptr;
}

FloodPacketDescriptor::~FloodPacketDescriptor()
{
    delete[] propertyNames;
}

bool FloodPacketDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<FloodPacket *>(obj)!=nullptr;
}

const char **FloodPacketDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *FloodPacketDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int FloodPacketDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 9+base->getFieldCount() : 9;
}

unsigned int FloodPacketDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_src
        FD_ISEDITABLE,    // FIELD_dst
        FD_ISEDITABLE,    // FIELD_lastHop
        FD_ISEDITABLE,    // FIELD_packetId
        FD_ISEDITABLE,    // FIELD_hopLimit
        FD_ISEDITABLE,    // FIELD_hopStart
        FD_ISEDITABLE,    // FIELD_wantAck
        FD_ISEDITABLE,    // FIELD_txTimeNs
        FD_ISEDITABLE,    // FIELD_payload
    };
    return (field >= 0 && field < 9) ? fieldTypeFlags[field] : 0;
}

const char *FloodPacketDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "src",
        "dst",
        "lastHop",
        "packetId",
        "hopLimit",
        "hopStart",
        "wantAck",
        "txTimeNs",
        "payload",
    };
    return (field >= 0 && field < 9) ? fieldNames[field] : nullptr;
}

int FloodPacketDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "src") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "dst") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "lastHop") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "packetId") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "hopLimit") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "hopStart") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "wantAck") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "txTimeNs") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "payload") == 0) return baseIndex + 8;
    return base ? base->findField(fieldName) : -1;
}

const char *FloodPacketDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "uint32_t",    // FIELD_src
        "uint32_t",    // FIELD_dst
        "uint32_t",    // FIELD_lastHop
        "uint32_t",    // FIELD_packetId
        "uint8_t",    // FIELD_hopLimit
        "uint8_t",    // FIELD_hopStart
        "bool",    // FIELD_wantAck
        "uint64_t",    // FIELD_txTimeNs
        "string",    // FIELD_payload
    };
    return (field >= 0 && field < 9) ? fieldTypeStrings[field] : nullptr;
}

const char **FloodPacketDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *FloodPacketDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int FloodPacketDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void FloodPacketDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'FloodPacket'", field);
    }
}

const char *FloodPacketDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string FloodPacketDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        case FIELD_src: return ulong2string(pp->getSrc());
        case FIELD_dst: return ulong2string(pp->getDst());
        case FIELD_lastHop: return ulong2string(pp->getLastHop());
        case FIELD_packetId: return ulong2string(pp->getPacketId());
        case FIELD_hopLimit: return ulong2string(pp->getHopLimit());
        case FIELD_hopStart: return ulong2string(pp->getHopStart());
        case FIELD_wantAck: return bool2string(pp->getWantAck());
        case FIELD_txTimeNs: return uint642string(pp->getTxTimeNs());
        case FIELD_payload: return oppstring2string(pp->getPayload());
        default: return "";
    }
}

void FloodPacketDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        case FIELD_src: pp->setSrc(string2ulong(value)); break;
        case FIELD_dst: pp->setDst(string2ulong(value)); break;
        case FIELD_lastHop: pp->setLastHop(string2ulong(value)); break;
        case FIELD_packetId: pp->setPacketId(string2ulong(value)); break;
        case FIELD_hopLimit: pp->setHopLimit(string2ulong(value)); break;
        case FIELD_hopStart: pp->setHopStart(string2ulong(value)); break;
        case FIELD_wantAck: pp->setWantAck(string2bool(value)); break;
        case FIELD_txTimeNs: pp->setTxTimeNs(string2uint64(value)); break;
        case FIELD_payload: pp->setPayload((value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FloodPacket'", field);
    }
}

omnetpp::cValue FloodPacketDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        case FIELD_src: return (omnetpp::intval_t)(pp->getSrc());
        case FIELD_dst: return (omnetpp::intval_t)(pp->getDst());
        case FIELD_lastHop: return (omnetpp::intval_t)(pp->getLastHop());
        case FIELD_packetId: return (omnetpp::intval_t)(pp->getPacketId());
        case FIELD_hopLimit: return (omnetpp::intval_t)(pp->getHopLimit());
        case FIELD_hopStart: return (omnetpp::intval_t)(pp->getHopStart());
        case FIELD_wantAck: return pp->getWantAck();
        case FIELD_txTimeNs: return (omnetpp::intval_t)(pp->getTxTimeNs());
        case FIELD_payload: return pp->getPayload();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'FloodPacket' as cValue -- field index out of range?", field);
    }
}

void FloodPacketDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        case FIELD_src: pp->setSrc(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_dst: pp->setDst(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_lastHop: pp->setLastHop(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_packetId: pp->setPacketId(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_hopLimit: pp->setHopLimit(omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        case FIELD_hopStart: pp->setHopStart(omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        case FIELD_wantAck: pp->setWantAck(value.boolValue()); break;
        case FIELD_txTimeNs: pp->setTxTimeNs(omnetpp::checked_int_cast<uint64_t>(value.intValue())); break;
        case FIELD_payload: pp->setPayload(value.stringValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FloodPacket'", field);
    }
}

const char *FloodPacketDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr FloodPacketDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void FloodPacketDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    FloodPacket *pp = omnetpp::fromAnyPtr<FloodPacket>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FloodPacket'", field);
    }
}

Register_Class(FloodHello)

FloodHello::FloodHello() : ::inet::FieldsChunk()
{
}

FloodHello::FloodHello(const FloodHello& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

FloodHello::~FloodHello()
{
    delete [] this->neighbours;
    delete [] this->mprs;
}

FloodHello& FloodHello::operator=(const FloodHello& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void FloodHello::copy(const FloodHello& other)
{
    this->src = other.src;
    delete [] this->neighbours;
    this->neighbours = (other.neighbours_arraysize==0) ? nullptr : new uint32_t[other.neighbours_arraysize];
    neighbours_arraysize = other.neighbours_arraysize;
    for (size_t i = 0; i < neighbours_arraysize; i++) {
        this->neighbours[i] = other.neighbours[i];
    }
    delete [] this->mprs;
    this->mprs = (other.mprs_arraysize==0) ? nullptr : new uint32_t[other.mprs_arraysize];
    mprs_arraysize = other.mprs_arraysize;
    for (size_t i = 0; i < mprs_arraysize; i++) {
        this->mprs[i] = other.mprs[i];
    }
}

void FloodHello::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->src);
    b->pack(neighbours_arraysize);
    doParsimArrayPacking(b,this->neighbours,neighbours_arraysize);
    b->pack(mprs_arraysize);
    doParsimArrayPacking(b,this->mprs,mprs_arraysize);
}

void FloodHello::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->src);
    delete [] this->neighbours;
    b->unpack(neighbours_arraysize);
    if (neighbours_arraysize == 0) {
        this->neighbours = nullptr;
    } else {
        this->neighbours = new uint32_t[neighbours_arraysize];
        doParsimArrayUnpacking(b,this->neighbours,neighbours_arraysize);
    }
    delete [] this->mprs;
    b->unpack(mprs_arraysize);
    if (mprs_arraysize == 0) {
        this->mprs = nullptr;
    } else {
        this->mprs = new uint32_t[mprs_arraysize];
        doParsimArrayUnpacking(b,this->mprs,mprs_arraysize);
    }
}

uint32_t FloodHello::getSrc() const
{
    return this->src;
}

void FloodHello::setSrc(uint32_t src)
{
    handleChange();
    this->src = src;
}

size_t FloodHello::getNeighboursArraySize() const
{
    return neighbours_arraysize;
}

uint32_t FloodHello::getNeighbours(size_t k) const
{
    if (k >= neighbours_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)neighbours_arraysize, (unsigned long)k);
    return this->neighbours[k];
}

void FloodHello::setNeighboursArraySize(size_t newSize)
{
    handleChange();
    uint32_t *neighbours2 = (newSize==0) ? nullptr : new uint32_t[newSize];
    size_t minSize = neighbours_arraysize < newSize ? neighbours_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        neighbours2[i] = this->neighbours[i];
    for (size_t i = minSize; i < newSize; i++)
        neighbours2[i] = 0;
    delete [] this->neighbours;
    this->neighbours = neighbours2;
    neighbours_arraysize = newSize;
}

void FloodHello::setNeighbours(size_t k, uint32_t neighbours)
{
    if (k >= neighbours_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)neighbours_arraysize, (unsigned long)k);
    handleChange();
    this->neighbours[k] = neighbours;
}

void FloodHello::insertNeighbours(size_t k, uint32_t neighbours)
{
    if (k > neighbours_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)neighbours_arraysize, (unsigned long)k);
    handleChange();
    size_t newSize = neighbours_arraysize + 1;
    uint32_t *neighbours2 = new uint32_t[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        neighbours2[i] = this->neighbours[i];
    neighbours2[k] = neighbours;
    for (i = k + 1; i < newSize; i++)
        neighbours2[i] = this->neighbours[i-1];
    delete [] this->neighbours;
    this->neighbours = neighbours2;
    neighbours_arraysize = newSize;
}

void FloodHello::appendNeighbours(uint32_t neighbours)
{
    insertNeighbours(neighbours_arraysize, neighbours);
}

void FloodHello::eraseNeighbours(size_t k)
{
    if (k >= neighbours_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)neighbours_arraysize, (unsigned long)k);
    handleChange();
    size_t newSize = neighbours_arraysize - 1;
    uint32_t *neighbours2 = (newSize == 0) ? nullptr : new uint32_t[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        neighbours2[i] = this->neighbours[i];
    for (i = k; i < newSize; i++)
        neighbours2[i] = this->neighbours[i+1];
    delete [] this->neighbours;
    this->neighbours = neighbours2;
    neighbours_arraysize = newSize;
}

size_t FloodHello::getMprsArraySize() const
{
    return mprs_arraysize;
}

uint32_t FloodHello::getMprs(size_t k) const
{
    if (k >= mprs_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)mprs_arraysize, (unsigned long)k);
    return this->mprs[k];
}

void FloodHello::setMprsArraySize(size_t newSize)
{
    handleChange();
    uint32_t *mprs2 = (newSize==0) ? nullptr : new uint32_t[newSize];
    size_t minSize = mprs_arraysize < newSize ? mprs_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        mprs2[i] = this->mprs[i];
    for (size_t i = minSize; i < newSize; i++)
        mprs2[i] = 0;
    delete [] this->mprs;
    this->mprs = mprs2;
    mprs_arraysize = newSize;
}

void FloodHello::setMprs(size_t k, uint32_t mprs)
{
    if (k >= mprs_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)mprs_arraysize, (unsigned long)k);
    handleChange();
    this->mprs[k] = mprs;
}

void FloodHello::insertMprs(size_t k, uint32_t mprs)
{
    if (k > mprs_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)mprs_arraysize, (unsigned long)k);
    handleChange();
    size_t newSize = mprs_arraysize + 1;
    uint32_t *mprs2 = new uint32_t[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        mprs2[i] = this->mprs[i];
    mprs2[k] = mprs;
    for (i = k + 1; i < newSize; i++)
        mprs2[i] = this->mprs[i-1];
    delete [] this->mprs;
    this->mprs = mprs2;
    mprs_arraysize = newSize;
}

void FloodHello::appendMprs(uint32_t mprs)
{
    insertMprs(mprs_arraysize, mprs);
}

void FloodHello::eraseMprs(size_t k)
{
    if (k >= mprs_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)mprs_arraysize, (unsigned long)k);
    handleChange();
    size_t newSize = mprs_arraysize - 1;
    uint32_t *mprs2 = (newSize == 0) ? nullptr : new uint32_t[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        mprs2[i] = this->mprs[i];
    for (i = k; i < newSize; i++)
        mprs2[i] = this->mprs[i+1];
    delete [] this->mprs;
    this->mprs = mprs2;
    mprs_arraysize = newSize;
}

class FloodHelloDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_src,
        FIELD_neighbours,
        FIELD_mprs,
    };
  public:
    FloodHelloDescriptor();
    virtual ~FloodHelloDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(FloodHelloDescriptor)

FloodHelloDescriptor::FloodHelloDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(flood::FloodHello)), "inet::FieldsChunk")
{
    propertyNames = nullptr;
}

FloodHelloDescriptor::~FloodHelloDescriptor()
{
    delete[] propertyNames;
}

bool FloodHelloDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<FloodHello *>(obj)!=nullptr;
}

const char **FloodHelloDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *FloodHelloDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int FloodHelloDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 3+base->getFieldCount() : 3;
}

unsigned int FloodHelloDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_src
        FD_ISARRAY | FD_ISEDITABLE | FD_ISRESIZABLE,    // FIELD_neighbours
        FD_ISARRAY | FD_ISEDITABLE | FD_ISRESIZABLE,    // FIELD_mprs
    };
    return (field >= 0 && field < 3) ? fieldTypeFlags[field] : 0;
}

const char *FloodHelloDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "src",
        "neighbours",
        "mprs",
    };
    return (field >= 0 && field < 3) ? fieldNames[field] : nullptr;
}

int FloodHelloDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "src") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "neighbours") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "mprs") == 0) return baseIndex + 2;
    return base ? base->findField(fieldName) : -1;
}

const char *FloodHelloDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "uint32_t",    // FIELD_src
        "uint32_t",    // FIELD_neighbours
        "uint32_t",    // FIELD_mprs
    };
    return (field >= 0 && field < 3) ? fieldTypeStrings[field] : nullptr;
}

const char **FloodHelloDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *FloodHelloDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int FloodHelloDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        case FIELD_neighbours: return pp->getNeighboursArraySize();
        case FIELD_mprs: return pp->getMprsArraySize();
        default: return 0;
    }
}

void FloodHelloDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        case FIELD_neighbours: pp->setNeighboursArraySize(size); break;
        case FIELD_mprs: pp->setMprsArraySize(size); break;
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'FloodHello'", field);
    }
}

const char *FloodHelloDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string FloodHelloDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        case FIELD_src: return ulong2string(pp->getSrc());
        case FIELD_neighbours: return ulong2string(pp->getNeighbours(i));
        case FIELD_mprs: return ulong2string(pp->getMprs(i));
        default: return "";
    }
}

void FloodHelloDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        case FIELD_src: pp->setSrc(string2ulong(value)); break;
        case FIELD_neighbours: pp->setNeighbours(i,string2ulong(value)); break;
        case FIELD_mprs: pp->setMprs(i,string2ulong(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FloodHello'", field);
    }
}

omnetpp::cValue FloodHelloDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        case FIELD_src: return (omnetpp::intval_t)(pp->getSrc());
        case FIELD_neighbours: return (omnetpp::intval_t)(pp->getNeighbours(i));
        case FIELD_mprs: return (omnetpp::intval_t)(pp->getMprs(i));
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'FloodHello' as cValue -- field index out of range?", field);
    }
}

void FloodHelloDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        case FIELD_src: pp->setSrc(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_neighbours: pp->setNeighbours(i,omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_mprs: pp->setMprs(i,omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FloodHello'", field);
    }
}

const char *FloodHelloDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr FloodHelloDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void FloodHelloDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    FloodHello *pp = omnetpp::fromAnyPtr<FloodHello>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'FloodHello'", field);
    }
}

}  // namespace flood

namespace omnetpp {

}  // namespace omnetpp

