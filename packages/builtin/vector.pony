class Vector[A, _alloc: USize]
  var _ptr: Pointer[A]
  var _size: USize

  new create() =>
    """
    Create an vector with zero elements, but space for len elements.
    """
    _ptr = Pointer[A]._alloc(_alloc)
    _size = 0

  new init(from: A^) =>
    """
    Create an vector of len elements, all initialised to the given value.
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
    Create an vector of len elements, populating them with random memory. This
    is only allowed for an vector of numbers.
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

  //TODO: insert does dynamic memory management and creates extra memory
  // when not necessary, fix this

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _ptr._update(i, consume value)
    else
      error
    end

  fun ref insert(i: USize, value: A, env: Env): Vector[A, _alloc]^ ? =>
    """
    Insert an element into the vector. Elements after this are moved up by one
    index.
    An out of bounds index raises an error.
    Inserting more elements into the vector than allocated raises an error.
    The vector is returned to allow call chaining.
    """
    if (i <= _size) and (_size < _alloc) then
      env.out.print((_size - i).string())
      _ptr._offset(i)._insert(1, _size - i)
      _ptr._update(i, consume value)
      _size = _size + 1
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
      _size = _size - 1
      _ptr._offset(i)._delete(1, _size - i)
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
/* To implement

  fun copy_to(dst: Vector[this->A!, _alloc], src_idx: USize, dst_idx: USize,
    len: USize): this->Vector[A, _alloc]^
  =>
    """
    Copy len elements from this(src_idx) to dst(dst_idx).
    The vector is returned to allow call chaining.
    """
    dst.reserve(dst_idx + len)
    _ptr._offset(src_idx)._copy_to(dst._ptr._offset(dst_idx), len)

    if dst._size < (dst_idx + len) then
      dst._size = dst_idx + len
    end
    this

  fun ref remove(i: USize, n: USize): Array[A]^ =>
    """
    Remove n elements from the vector, beginning at index i.
    The vector is returned to allow call chaining.
    """
    if i < _size then
      let count = n.min(_size - i)
      _size = _size - count
      _ptr._offset(i)._delete(count, _size - i)
    end
    this

  fun ref clear(): Array[A]^ =>
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

  fun ref unshift(value: A): Array[A]^ =>
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
    reserve(_size + 1)
    _ptr._update(_size, consume value)
    _size = _size + 1
    this

  fun ref append[m: USize](vec: Vector[A, m]): Vector[A, #(_alloc + m)] =>
    """
    Append the elements from a second vector.
    A new vector whose size is of the two vectors is returned.
    """
    let new_vec = Vector[A, #(_alloc + m)]
    var i: USize = 0
    while i < _alloc do
      try
        new_vec.insert(i, this(i))
      else
        error
      end
    end
*/
