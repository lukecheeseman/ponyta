class Vector[A, _alloc: USize]
  var _ptr: Pointer[A]
  var _size: USize

  new create() =>
    """
    Create an array with zero elements, but space for len elements.
    """
    _ptr = Pointer[A]._alloc(_alloc)
    _size = 0

  new init(from: A^) =>
    """
    Create an array of len elements, all initialised to the given value.
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
    Create an array of len elements, populating them with random memory. This
    is only allowed for an array of numbers.
    """
    _ptr = Pointer[A]._alloc(_alloc)
    _size = _alloc

  fun size(): USize =>
    """
    The number of elements in the array.
    """
    _size

  fun space(): USize =>
    """
    The maximum number of elements in the array
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
    index, extending the vector.
    An out of bounds index raises an error.
    Inserting more elements into the vector than allocated raises an error.
    The vector is returned to allow call chaining.
    """
    if (i <= _size) and ((_size + 1) < _alloc) then
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
