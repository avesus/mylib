#ifndef _SMALL_STR_H_
#define _SMALL_STR_H_

#include <helpers/util.h>
#include <string>

#define SS_DEBUG
#ifdef SS_DEBUG
#define SS_ASSERT(X) assert(X)
#else
#define SS_ASSERT(X)
#endif

// default to 16 on 64bit linux
#define SS_PTR_SHIFT (64 - VM_ADDRESS_BITS)

// default to 20 with 64bit linux glibc 64bit malloc
#define SS_META_DATA_BITS (SS_PTR_SHIFT + MALLOC_ALIGNMENT_BITS)

// 19 bits for size by default
#define SS_SIZE_BITS (SS_META_DATA_BITS - 1)

// 19th bit will tell if buffer is allocated internally or passed from other
#define SS_INTERNAL_ALLOC_MASK (1 << SS_SIZE_BITS)

// size mask is just standard...
#define SS_SIZE_MASK (((1UL) << SS_SIZE_BITS) - 1)

// ptr mask is all bits excluding size and meta data.
#define SS_PTR_MASK (~(SS_SIZE_MASK | SS_INTERNAL_ALLOC_MASK))

#define SS_PLACE_PTR(X) (((uint64_t)(X)) << (SS_PTR_SHIFT))

class small_string {
    // data layout of 64 bit value is:
    //[63 - 20]        [19]                 [18 - 0]
    //[Ptr to string]  [Need to free flag]  [size/length of string]

    // 20 bits for meta data is because 48 bits for VM address means 16 free
    // bits in 64 bit + 4 additional bits from malloc/alloc allignment

    uint64_t
    realloc_ptr(const uint64_t alloc_size) const {
        // take MIN here to avoid unnecissary over allocation
        if (this->get_internal_alloc()) {
            return SS_PLACE_PTR((uint64_t)realloc(
                       this->get_str(),
                       MIN(MAX(alloc_size + 1, 16), (1 << SS_SIZE_BITS)))) |
                   SS_INTERNAL_ALLOC_MASK;
        }
        else {
            uint64_t ptr = (uint64_t)mymalloc(
                MIN(MAX(alloc_size + 1, 16), (1 << SS_SIZE_BITS)));
            memcpy((void *)ptr, this->get_str(), this->get_size());
            return SS_PLACE_PTR(ptr) | SS_INTERNAL_ALLOC_MASK;
        }
    }

    uint64_t
    malloc_ptr(const uint64_t alloc_size) const {
        this->free_str();
        // take MIN here to avoid unnecissary over allocation
        return SS_PLACE_PTR((uint64_t)malloc(
                   MIN(MAX(alloc_size + 1, 16), (1 << SS_SIZE_BITS)))) |
               SS_INTERNAL_ALLOC_MASK;
    }


   public:
    uint64_t ptr_and_size;

    small_string() {
        this->ptr_and_size = 0;
    }
    small_string(const uint64_t init_size) {
        SS_ASSERT(init_size <= SS_SIZE_MASK);
        this->ptr_and_size = this->malloc_ptr(init_size) | init_size;
    }

    ~small_string() {
        // really dislike destructors
    }

    void
    free_str() const {
        if (this->get_internal_alloc()) {
            myfree(this->get_str());
        }
    }

    constexpr uint64_t
    get_internal_alloc() const {
        return this->ptr_and_size & SS_INTERNAL_ALLOC_MASK;
    }

    constexpr uint32_t
    should_realloc(const uint32_t size) const {
        return (!this->get_internal_alloc()) ||
               (size + 1 >= GET_MALLOC_BLOCK_LENGTH(this->get_str()));
    }

    constexpr uint64_t
    get_size() const {
        return this->ptr_and_size & SS_SIZE_MASK;
    }


    char *
    get_str() const {
        return (char *)((this->ptr_and_size >> SS_META_DATA_BITS)
                        << MALLOC_ALIGNMENT_BITS);
    }

    void
    set_size(const uint64_t size) {
        this->ptr_and_size = (this->ptr_and_size & (~SS_SIZE_MASK)) | size;
    }

    void
    add_null_term() const {
        char * const str      = this->get_str();
        str[this->get_size()] = 0;
    }

    void
    operator+=(const char * const other) {
        const uint32_t size = this->get_size();
        char *         str  = this->get_str() + size;
        uint32_t       idx  = 0;
        while (other[idx]) {
            if (this->should_realloc(size + idx)) {
                this->ptr_and_size = this->realloc_ptr(2 * (size + idx));
                str                = this->get_str() + size;
            }
            str[idx] = other[idx];
            idx++;
        }
        this->ptr_and_size += idx;
        this->add_null_term();
        SS_ASSERT(this->get_internal_alloc());
    }


    void
    operator+=(small_string const & other) {
        SS_ASSERT(this->get_size() + other.get_size() <= SS_SIZE_MASK);
        const uint32_t size = this->get_size();
        if (this->should_realloc(size + other.get_size())) {
            this->ptr_and_size =
                this->realloc_ptr(MAX(2 * size, size + other.get_size())) |
                size;
        }
        memcpy(this->get_str() + size, other.get_str(), other.get_size());
        this->ptr_and_size += other.get_size();
        SS_ASSERT(this->get_internal_alloc());
    }

    void
    operator+=(std::string const & other) {
        SS_ASSERT(this->get_size() + other.length() <= SS_SIZE_MASK);
        const uint32_t size = this->get_size();
        if (this->should_realloc(size + other.length())) {
            this->ptr_and_size =
                this->realloc_ptr(MAX(2 * size, size + other.length())) | size;
        }
        memcpy(this->get_str() + size, other.c_str(), other.length());
        this->ptr_and_size += other.length();
        this->add_null_term();
        SS_ASSERT(this->get_internal_alloc());
    }


    constexpr uint32_t
    operator==(small_string const & other) const {
        return this->ptr_and_size == other.ptr_and_size ||
               ((this->get_size() == other.get_size()) &&
                (!memcmp(this->get_str(), other.get_str(), this->get_size())));
    }

    uint32_t
    operator==(std::string const & other) const {
        return (this->get_size() == other.length()) &&
               (!memcmp(this->get_str(), other.c_str(), other.length()));
    }


    void
    operator=(const char * const other) {
        if (((uint64_t)other) & MALLOC_ALIGNMENT_MASK) {
            this->dcopy(other);
            return;
        }
        const uint32_t size = strlen(other);
        SS_ASSERT(size <= SS_SIZE_MASK);
        this->free_str();
        this->ptr_and_size = SS_PLACE_PTR(other) | size;
        SS_ASSERT(!this->get_internal_alloc());
    }

    void
    operator=(small_string const & other) {
        SS_ASSERT(other.get_size() <= SS_SIZE_MASK);
        this->free_str();
        this->ptr_and_size = other.ptr_and_size & (~SS_INTERNAL_ALLOC_MASK);
        SS_ASSERT(this->get_str() == other.get_str());
        SS_ASSERT(!this->get_internal_alloc());
    }

    void
    operator=(std ::string const & other) {
        SS_ASSERT(other.length() <= SS_SIZE_MASK);
        this->free_str();
        this->ptr_and_size = SS_PLACE_PTR(other.c_str()) | other.length();
        SS_ASSERT(this->get_str() == other.c_str());
        SS_ASSERT(!this->get_internal_alloc());
    }

    void
    dcopy(const char * const other) {
        char *   str = this->get_str();
        uint32_t idx = 0;
        while (other[idx]) {
            if (this->should_realloc(idx)) {
                this->ptr_and_size = this->malloc_ptr(2 * (idx));
                str                = this->get_str();
            }
            str[idx] = other[idx];
            idx++;
        }
        this->set_size(idx);
        this->add_null_term();
        SS_ASSERT(this->get_internal_alloc());
    }

    void
    dcopy(small_string const & other) {
        SS_ASSERT(other.get_size() <= SS_SIZE_MASK);
        if (this->should_realloc(other.get_size())) {
            this->ptr_and_size =
                this->malloc_ptr(other.get_size()) | other.get_size();
        }
        memcpy(this->get_str(), other.get_str(), other.get_size());
        this->add_null_term();
        SS_ASSERT(this->get_internal_alloc());
    }

    void
    dcopy(std ::string const & other) {
        SS_ASSERT(other.length() <= SS_SIZE_MASK);
        if (this->should_realloc(other.length())) {
            this->ptr_and_size =
                this->malloc_ptr(other.length()) | other.length();
        }
        memcpy(this->get_str(), other.c_str(), other.length());
        this->add_null_term();
        SS_ASSERT(this->get_internal_alloc());
    }
};


#endif
