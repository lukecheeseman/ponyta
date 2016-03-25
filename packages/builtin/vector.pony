class Vector[A, _alloc: USize]
  var _ptr: Pointer[A]
  var _size: USize

  new create() =>
    """
    Create a vector with zero elements, but space for len elements.
    """
    _ptr = Pointer[A]._alloc(_alloc)
    _size = 0

  new init(from: A^) =>
    """
    Create a vector of len elements, all initialised to the given value.
    """
    _ptr = Pointer[A]._alloc(_alloc)
    _size = _alloc

    var i: USize = 0

    while i < _alloc do
      _ptr._update(i, from)
      i = i + 1
    end

  new undefined[B: (A & Real[B] val & Number) = A]() =>
    """
    Create a vector of len elements, populating them with random memory. This
    is only allowed for a vector of numbers.
    """
    _ptr = Pointer[A]._alloc(_alloc)
    _size = _alloc

  fun size(): USize =>
    """
    The number of elements in the vector.
    """
    _size

  fun space(): USize =>
    """
    The maximum number of elements in the vector
    """
    _alloc

  fun apply(i: USize): this->A^ ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _ptr._apply(i)
    else
      error
    end

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _ptr._update(i, consume value)
    else
      error
    end

  fun ref insert(i: USize, value: A): Vector[A, _alloc]^ ? =>
    """
    Insert an element into the vector. Elements after this are moved up by one
    index.
    An out of bounds index raises an error.
    Inserting more elements into the vector than allocated raises an error.
    The vector is returned to allow call chaining.
    """
    if (i <= _size) and (_size < _alloc) then
      var move = i
      while move < _size do
        _ptr._update(move + 1, _ptr._apply(move))
        move = move + 1
      end
      _ptr._update(i, consume value)
      _size = _size + 1
      this
    else
      error
    end

  fun ref truncate(len: USize): Vector[A, _alloc]^ =>
    """
    Truncate an vector to the given length, discarding excess elements. If the
    vector is already smaller than len, do nothing.
    The vector is returned to allow call chaining.
    """
    _size = _size.min(len)
    this

  fun copy_to[m: USize](dst: Vector[this->A!, m], src_idx: USize, dst_idx: USize,
    len: USize): this->Vector[A, _alloc]^ ?
  =>
    """
    Copy len elements from this(src_idx) to dst(dst_idx).
    The vector is returned to allow call chaining.
    """
    if (dst_idx + len) < m then
      _ptr._offset(src_idx)._copy_to(dst._ptr._offset(dst_idx), len)

      if dst._size < (dst_idx + len) then
        dst._size = dst_idx + len
      end
      this
    else
      error
    end

  fun ref remove(i: USize, n: USize): Vector[A, _alloc]^ ? =>
    """
    Remove n elements from the vector, beginning at index i.
    The vector is returned to allow call chaining.
    """
    if i < _size then
      let count = n.min(_size - i)
      var move = i
      while move < (_size - count) do
        _ptr._update(move, _ptr._apply(move + count))
        move = move + 1
      end
      _size = _size - count
      this
    else
      error
    end

  fun ref delete(i: USize): A^ ? =>
    """
    Delete an element from the vector. Elements after this are moved down by one
    index, compacting the vector.
    An out of bounds index raises an error.
    The deleted element is returned.
    """
    if i < _size then
      let deleted = apply(i)
      remove(i, 1)
      deleted
    else
      error
    end


  fun ref clear(): Vector[A, _alloc]^ =>
    """
    Remove all elements from the vector.
    The vector is returned to allow call chaining.
    """
    _size = 0
    this

  fun ref pop(): A^ ? =>
    """
    Remove an element from the end of the vector.
    The removed element is returned.
    """
    delete(_size - 1)

  fun ref unshift(value: A): Vector[A, _alloc]^ =>
    """
    Add an element to the beginning of the vector.
    The vector is returned to allow call chaining.
    """
    try
      insert(0, consume value)
    end
    this

  fun ref shift(): A^ ? =>
    """
    Remove an element from the beginning of the vector.
    The removed element is returned.
    """
    delete(0)

  fun ref push(value: A): Vector[A, _alloc]^ =>
    """
    Add an element to the end of the vector.
    The vector is returned to allow call chaining.
    """
    _ptr._update(_size, consume value)
    _size = _size + 1
    this

  fun ref append[m: USize](vector: Vector[A, m]): Vector[A, #(_alloc + m)]^ ? =>
    """
    Append the elements from a second vector.
    A new vector whose size is of the two vectors is returned.
    """
    let new_vector = Vector[A, #(_alloc + m)]
    try
      var i: USize = 0
      while i < _alloc do
        new_vector.push(apply(i))
        i = i + 1
      end

      while i < #(_alloc + m) do
        new_vector.push(vector(i - _alloc))
        i = i + 1
      end
    else
      error
    end
    new_vector

  // TODO: more methods required still

  fun keys(): VectorKeys[A, _alloc, this->Vector[A, _alloc]]^ =>
    """
    Return an iterator over the indices in the vector.
    """
    VectorKeys[A, _alloc, this->Vector[A, _alloc]](this)

  fun values(): VectorValues[A, _alloc, this->Vector[A, _alloc]]^ =>
    """
    Return an iterator over the values in the vector.
    """
    VectorValues[A, _alloc, this->Vector[A, _alloc]](this)

  fun pairs(): VectorPairs[A, _alloc, this->Vector[A, _alloc]]^ =>
    """
    Return an iterator over the (index, value) pairs in the vector.
    """
    VectorPairs[A, _alloc, this->Vector[A, _alloc]](this)

class VectorKeys[A, _alloc: USize, B: Vector[A, _alloc] #read] is Iterator[USize]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < _vector.size()

  fun ref next(): USize =>
    if _i < _vector.size() then
      _i = _i + 1
    else
      _i
    end

class VectorValues[A, _alloc: USize, B: Vector[A, _alloc] #read] is Iterator[B->A]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < _vector.size()

  fun ref next(): B->A ? =>
    _vector(_i = _i + 1)

class VectorPairs[A, _alloc: USize, B: Vector[A, _alloc] #read] is Iterator[(USize, B->A)]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < _vector.size()

  fun ref next(): (USize, B->A) ? =>
    (_i, _vector(_i = _i + 1))

