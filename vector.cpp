#pragma once

#include <initializer_list>
#include <algorithm>
#include <utility>

template <typename T>
class Vector;

template <typename T>
class Iterator;

constexpr size_t BLOCK_SIZE = 512;

template <typename T>
class Vector {

    // Вспомогательный класс индексации по внутреннему содержимому вектора ref_ (о нём ниже):
    //     ps_ - пара из {номер блока, позиция в блоке}.
    //
    //     ref_sz_ - количество блоков (как видно, при адресации блоков оно существенно используется и меняется в
    //               процессе работы, например, при реаллокации вектора, поэтому удобно хранить как поле класса).
    
    class Pointer {
    public:
        Pointer() : ps_({0, 0}), ref_sz_(0) {
        }

        Pointer(std::pair<size_t, size_t> p, size_t ref_size)
            : ps_(std::move(p)), ref_sz_(ref_size) {
        }

        // Переход к следующему элементу вектора
        Pointer& operator++() {
            if (ps_.second + 1 == BLOCK_SIZE) {
                ps_ = std::make_pair((ps_.first + 1) % ref_sz_, 0);
                return *this;
            }

            ps_ = std::make_pair(ps_.first, ps_.second + 1);
            return *this;
        }

        // Переход к предыдущему элементу вектора
        Pointer& operator--() {
            if (ps_.second == 0) {
                ps_ = std::make_pair(ps_.first ? ps_.first - 1 : ref_sz_ - 1, BLOCK_SIZE - 1);
                return *this;
            }

            ps_ = std::make_pair(ps_.first, ps_.second - 1);
            return *this;
        }

        // Метод сдвига по вектору вправо на x единиц
        Pointer operator+(size_t x) const {
            Pointer cur = *this;
            size_t mp = (cur.ps_.second + x) / BLOCK_SIZE;

            cur.ps_.second = (cur.ps_.second + x) % BLOCK_SIZE;
            cur.ps_.first = (cur.ps_.first + mp) % ref_sz_;

            return cur;
        }

        bool operator==(const Pointer& other) const {  // consider ref_sz's are equivalent
            return ps_ == other.ps_;
        }

        bool operator!=(const Pointer& other) const {  // consider ref_sz's are equivalent
            return ps_ != other.ps_;
        }

        bool operator<=(const Pointer& other) const {  // consider ref_sz's are equivalent
            return ps_.first == other.ps_.first && ps_.second <= other.ps_.second;
        }

    private:
        size_t X() const {
            return ps_.first;
        }

        size_t Y() const {
            return ps_.second;
        }

        std::pair<size_t, size_t> ps_;
        size_t ref_sz_;

        friend class Vector;
    };

public:
    Vector() {
        ref_sz_ = 1;
        begin_ = Pointer({0, 0}, ref_sz_);
        end_ = Pointer({0, 0}, ref_sz_);
        size_ = 0;

        ref_ = new T*[ref_sz_];
        ref_[0] = new T[BLOCK_SIZE];

        --end_;
    }

    Vector(const Vector& rhs) : Vector() {
        for (size_t i = 0; i < rhs.Size(); ++i) {
            PushBack(rhs[i]);
        }
    }

    Vector(Vector&& rhs) noexcept : Vector() {
        for (size_t i = 0; i < rhs.Size(); ++i) {
            PushBack(rhs[i]);
        }

        rhs.Clear();
    }

    explicit Vector(size_t size) : Vector() {
        for (size_t i = 0; i < size; ++i) {
            PushBack(T());
        }
    }

    Vector(std::initializer_list<T> list) : Vector() {
        for (auto& item : list) {
            PushBack(item);
        }
    }

    Vector& operator=(Vector rhs) {
        if (ref_ == rhs.ref_) {
            ref_sz_ = 1;
            begin_ = Pointer({0, 0}, ref_sz_);
            end_ = Pointer({0, 0}, ref_sz_);
            size_ = 0;

            ref_ = new T*[ref_sz_];
            ref_[0] = new T[BLOCK_SIZE];

            --end_;

            for (size_t i = 0; i < rhs.Size(); ++i) {
                PushBack(rhs[i]);
            }

            return *this;
        }

        Swap(rhs);
        return *this;
    }

    ~Vector() {
        Clear();

        for (size_t i = 0; i < ref_sz_; ++i) {
            if (ref_[i] != nullptr) {
                delete[] ref_[i];
            }
        }

        delete[] ref_;
    }

    void Swap(Vector& rhs) {
        std::swap(ref_, rhs.ref_);
        std::swap(ref_sz_, rhs.ref_sz_);
        std::swap(begin_, rhs.begin_);
        std::swap(end_, rhs.end_);
        std::swap(size_, rhs.size_);
    }

    void PushBack(T value) {
        ++end_;
        if (size_ > 0 && end_ <= begin_) {
            --end_;
            Realloc();
            ++end_;
        }

        if (ref_[end_.X()] == nullptr) {
            ref_[end_.X()] = new T[BLOCK_SIZE];
        }

        ref_[end_.X()][end_.Y()] = std::move(value);
        ++size_;
    }

    void PopBack() {
        auto pr = end_;
        --end_;

        if (size_ > 1 && end_.X() != pr.X()) {
            delete[] ref_[pr.X()];
            ref_[pr.X()] = nullptr;
        }

        --size_;
    }

    void PushFront(T value) {
        --begin_;
        if (size_ > 0 && end_ <= begin_) {
            ++begin_;
            Realloc();
            --begin_;
        }

        if (ref_[begin_.X()] == nullptr) {
            ref_[begin_.X()] = new T[BLOCK_SIZE];
        }

        ref_[begin_.X()][begin_.Y()] = value;
        ++size_;
    }

    void PopFront() {
        auto pr = begin_;
        ++begin_;

        if (size_ > 1 && begin_.X() != pr.X()) {
            delete[] ref_[pr.X()];
            ref_[pr.X()] = nullptr;
        }

        --size_;
    }

    T& operator[](size_t ind) {
        auto ptr = Offset(ind);

        return Get(ptr);
    }

    T operator[](size_t ind) const {
        auto ptr = Offset(ind);

        return Get(ptr);
    }

    T& Front() {
        return (*this)[0];
    }

    T& Back() {
        return (*this)[size_ - 1];
    }

    size_t Size() const {
        return size_;
    }

    void Clear() {
        size_t sz = size_;
        for (size_t i = 0; i < sz; ++i) {
            PopBack();
        }
    }

    void Assign(const T& value) {
        for (auto &item : *this) {
            item = value;
        }
    }

    void Resize(size_t new_sz, const T& value = T()) {
        while (new_sz < size_) {
            PopBack();
        }

        while (new_sz > size_) {
            PushBack(value);
        }
    }

    void Insert(size_t pos, const T& value) {
        Insert(Iterator<T>(this, Offset(pos)), value);
    }

    void Insert(Iterator<T> pos, const T& value) {
        Vector copy;

        while (Begin() != pos) {
            auto front = Front();

            copy.PushBack(front);

            PopFront();
        }

        PushFront(value);

        for (size_t i = 0; i < copy.Size(); ++i) {
            PushFront(copy[copy.Size() - i - 1]);
        }
    }

    void Erase(size_t pos) {
        Erase(Iterator<T>(this, Offset(pos)));
    }

    void Erase(Iterator<T> pos) {
        Vector copy;

        while (Begin() != pos) {
            auto front = Front();

            copy.PushBack(front);

            PopFront();
        }

        PopFront();

        for (size_t i = 0; i < copy.Size(); ++i) {
            PushFront(copy[copy.Size() - i - 1]);
        }
    }

    Iterator<T> Begin() {
        Pointer begin = begin_;

        return Iterator<T>(this, begin);
    }

    Iterator<T> End() {
        auto end = end_;
        ++end;

        return Iterator<T>(this, end);
    }

private:
    // Двумерный массив из блоков по BLOCK_SIZE = 512 элементов.
    T** ref_ = nullptr;

    size_t ref_sz_;
    
    // [begin, end] - конкретно здесь обе границы включительно. У методов же Begin() и End() поведение стандартно:
    // вектор в диапазоне [Begin(), End()), то есть конец End() невключительно.
    Pointer begin_, end_;

    // Реальный размер
    size_t size_;

    void Realloc() {
        auto ref_prev = ref_;

        size_t new_ref_sz = 2 * ref_sz_;

        ref_ = new T*[new_ref_sz];
        std::fill(ref_, ref_ + new_ref_sz, nullptr);

        size_t j = 0;
        for (size_t i = begin_.X(); i != end_.X(); i = (i + 1) % ref_sz_, ++j) {
            ref_[j] = ref_prev[i];
        }
        ref_[j] = ref_prev[end_.X()];

        ref_sz_ = new_ref_sz;
        begin_ = Pointer(std::make_pair(0, begin_.Y()), ref_sz_);

        if (size_ > 0) {
            end_ = begin_ + (size_ - 1);
        } else {
            end_ = --begin_;
            ++begin_;
        }

        delete[] ref_prev;
    }

    Pointer Offset(size_t index) const {
        return begin_ + index;
    }

    T Get(const Pointer& ptr) const {
        return ref_[ptr.X()][ptr.Y()];
    }

    T& Get(const Pointer& ptr) {
        return ref_[ptr.X()][ptr.Y()];
    }

    friend class Iterator<T>;
    friend class Pointer;
};

template <typename T>
class Iterator {
    using IteratorTag = std::bidirectional_iterator_tag;

public:
    typedef T value_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef T& reference;
    typedef IteratorTag iterator_category;

    Iterator() = delete;
    explicit Iterator(Vector<T>* ref, Vector<T>::Pointer hook) : reference_(ref), ptr_(std::move(hook)) {
    }

    Iterator& operator--() {
        --ptr_;
        return *this;
    }

    Iterator operator--(int) {
        auto tmp = *this;
        --*this;
        return tmp;
    }

    Iterator& operator++() {
        ++ptr_;
        return *this;
    }

    Iterator operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    T& operator*() const {
        return reference_->Get(ptr_);
    }

    T* operator->() const {
        return &reference_->Get(ptr_);
    }

    bool operator==(const Iterator& rhs) const {
        return ptr_ == rhs.ptr_ && reference_ == rhs.reference_;
    }

    bool operator!=(const Iterator& rhs) const {
        return !(*this == rhs);
    }

private:
    Vector<T>::Pointer ptr_;
    Vector<T>* reference_ = nullptr;

    friend class Vector<T>;
};

template <typename T>
Iterator<T> begin(Vector<T>& list) {  // NOLINT
    return list.Begin();
}

template <typename T>
Iterator<T> end(Vector<T>& list) {  // NOLINT
    return list.End();
}

