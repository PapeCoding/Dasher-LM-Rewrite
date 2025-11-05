#pragma once

// Array class that holds pointers to children
// First child is stored directly, once a second child gets added we switch to an array on the heap and copy everything over
template<typename Element_t, typename Counter = unsigned>
class LazyReferenceArray{
public:
    Counter elementCount = 0;

    union {
        Element_t** array;
        Element_t* firstElement = nullptr;
    };

public:
    ~LazyReferenceArray(){
        if(elementCount > 1) delete[] array;
    }

    void Add(Element_t* elem){
        if(elementCount == 0){
            // 0 elements, so directly assign into the pointer
            firstElement = elem;
        }else if(elementCount == 1){
            // 1 element, so create array and copy pointer from before
            Element_t* firstChild = firstElement;
            array = new Element_t*[2];
            array[0] = firstChild;
            array[1] = elem;
        }else if(elementCount > 1){
            //extend array by one and copy over
            Element_t** oldElements = array;
            array = new Element_t*[elementCount + 1];
            memcpy(&array[0], &oldElements[0], elementCount * sizeof(Element_t*));
            delete[] oldElements;

            array[elementCount] = elem;
        }
        elementCount++;
    }

    class iterator {
    public:
        iterator(const LazyReferenceArray* array, Counter start): array(array), crr(start) {}
        iterator operator++() {crr++; return *this;}
        bool operator!=(const iterator & other) const { return crr != other.crr;  }
        Element_t* operator*() const {
            return (array->elementCount == 1) ? array->firstElement : array->array[crr];
        }
    private:
        Counter crr;
        const LazyReferenceArray* array;
    };

public:
   iterator begin() const { return iterator(this, 0); }
   iterator end() const { return iterator(this, this->elementCount); }
};