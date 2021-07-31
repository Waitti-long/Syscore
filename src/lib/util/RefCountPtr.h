#ifndef OS_RISC_V_REFCOUNTPTR_H
#define OS_RISC_V_REFCOUNTPTR_H

template<typename Ptr>
class RefCountPtr {
public:
    explicit RefCountPtr(Ptr *ptr) {
        atomic_count_ = new AtomicCount(ptr, 1);
    }

    virtual ~RefCountPtr() {
        decrease();
    }

    RefCountPtr(const RefCountPtr<Ptr> &other_ptr) : atomic_count_(other_ptr.atomic_count_) {
        increase();
    }

    RefCountPtr &operator=(RefCountPtr<Ptr> other_ptr) {
        decrease();
        atomic_count_ = other_ptr.atomic_count_;
        increase();
        return *this;
    }

    Ptr *operator->() {
        return atomic_count_->ptr();
    }

    Ptr operator*() {
        return *atomic_count_->ptr();
    }

private:
    void increase() {
        atomic_count_->inc();
    }

    void decrease() {
        atomic_count_->dec();
        if (atomic_count_ && atomic_count_->count() == 0) {
            release();
        }
    }

    void release() {
        delete atomic_count_;
    }

    class AtomicCount {
    public:
        AtomicCount(Ptr *ptr, int count) : ptr_(ptr), count_(count) {};

        virtual ~AtomicCount() {
            delete ptr_;
            ptr_ = nullptr;
        }

        void inc() {
            ++count_;
        }

        void dec() {
            --count_;
        }

        Ptr *ptr() {
            return ptr_;
        }

        int count() {
            return count_;
        }

    private:
        int count_;
        Ptr *ptr_;
    };

    AtomicCount *atomic_count_;
};

#endif //OS_RISC_V_REFCOUNTPTR_H