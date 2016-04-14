class Vector[A, size: USize]
  // we want to embed this pointer and magically initialise it once
  let _ptr: Pointer[A] = Pointer[A]._alloc(size)

  new init(from: Seq[A^]) ? =>
    """
    Create a vector of len elements, all initialised to the given value.
    """
    var i: USize = 0
    while i < size do
      _ptr._update(i, from(i))
      i = i + 1
    end

  new undefined[B: (A & Real[B] val & Number) = A]() =>
    """
    Create a vector of len elements, populating them with random memory. This
    is only allowed for a vector of numbers.
    """
    true

  fun apply(i: USize): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    if i < size then
      _ptr._apply(i)
    else
      error
    end

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    """
    if i < size then
      _ptr._update(i, consume value)
    else
      error
    end

  fun add[osize: USize](vector: Vector[this->A, osize]):
        Vector[this->A!, #(size + osize)]^ ? =>
    """
    Create a new vector with references to the contents of this
    vector and the argument vector.
    """
    let array = Array[this->A]
    for value in values() do
      array.push(value)
    end
    for value in vector.values() do
      array.push(value)
    end
    Vector[this->A!, #(size + osize)].init(array)


  fun keys(): VectorKeys[A, size, this->Vector[A, size]]^ =>
    """
    Return an iterator over the indices in the vector.
    """
    VectorKeys[A, size, this->Vector[A, size]]

  fun values(): VectorValues[A, size, this->Vector[A, size]]^ =>
    """
    Return an iterator over the values in the vector.
    """
    VectorValues[A, size, this->Vector[A, size]](this)

  fun pairs(): VectorPairs[A, size, this->Vector[A, size]]^ =>
    """
    Return an iterator over the pairs of indices and values in the vector.
    """
    VectorPairs[A, size, this->Vector[A, size]](this)

class VectorKeys[A, size: USize, B: Vector[A, size] #read] is Iterator[USize]
  var _i: USize

  new create() =>
    _i = 0

  fun has_next(): Bool =>
    _i < size

  fun ref next(): USize =>
    if _i < size then
      _i = _i + 1
    else
      _i
    end

class VectorValues[A, size: USize, B: Vector[A, size] #read] is Iterator[B->A]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < size

  fun ref next(): B->A ? =>
    _vector(_i = _i + 1)

class VectorPairs[A, size: USize, B: Vector[A, size] #read] is Iterator[(USize, B->A)]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < size

  fun ref next(): (USize, B->A) ? =>
    (_i, _vector(_i = _i + 1))
