class Vector[A, size: USize]
  var _ptr: Pointer[A]

  new init(from: Seq[A^]) ? =>
    """
    Create a vector of len elements, all initialised to the given value.
    """
    _ptr = Pointer[A]._alloc(size)

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
    _ptr = Pointer[A]._alloc(size)

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

  fun add[osize: USize](vector: Vector[this->A, osize]):
        Vector[this->A!, #(size + osize)]^ ? =>
    let array = Array[this->A]
    for value in values() do
      array.push(value)
    end
    for value in vector.values() do
      array.push(value)
    end
    Vector[this->A!, #(size + osize)].init(array)

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

/*

  fun ref append[m: USize](vector: Vector[A, m]): Vector[A, #(_alloc + m)]^ ? =>
    """
    Append the elements from a second vector.
    A new vector whose size is of the two vectors is returned.
    """
    let new_vector = Vector[A, #(_alloc + m)]
    try
      var i: USize = 0
      while i < _size do
        new_vector.push(apply(i))
        i = i + 1
      end

      i = 0
      while i < vector.size() do
        new_vector.push(vector(i))
        i = i + 1
      end
    else
      error
    end
    new_vector
    */
/*
//FIXME: going to remove many many methods here
// won't have things like delete
// going to be more functional

  new create() =>
    """
    Create a vector with zero elements, but space for len elements.
    """
    _ptr = Pointer[A]._alloc(_alloc)
    _size = 0


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



  // TODO: discuss whether we need this in a vector as we aren't managing
  // memory resizing
  fun ref insert(i: USize, value: A): Vector[A, _alloc]^ ? =>
    """
    Insert an element into the vector. Elements after this are moved up by one
    index.
    An out of bounds index raises an error.
    Inserting more elements into the vector than allocated raises an error.
    The vector is returned to allow call chaining.
    """
    if (i <= _size) and (_size < _alloc) then
      var move = _size
      while move > i do
        _ptr._update(move, _ptr._apply(move - 1))
        move = move - 1
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
    if (dst_idx + len) <= m then
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

  fun ref delete(i: USize): A ? =>
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
      while i < _size do
        new_vector.push(apply(i))
        i = i + 1
      end

      i = 0
      while i < vector.size() do
        new_vector.push(vector(i))
        i = i + 1
      end
    else
      error
    end
    new_vector

    */
