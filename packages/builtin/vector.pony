class Vector[A, _size: USize]

  new _create() =>
    """
    Create a vector of len elements, populating them with random memory.
    """
    true

  fun _apply(i: USize): this->A =>
    """
    Get the i-th element
    """
    compile_intrinsic

  fun ref _update(i: USize, value: A): A^ =>
    """
    Change the i-th element
    """
    compile_intrinsic

  new duplicate(from: A^) =>
    var i: USize = 0
    while i < _size do
      _update(i, from)
      i = i + 1
    end

  new init(from: Seq[A^]) ? =>
    """
    Create a vector of len elements, initialised from the given sequence.
    """
    var i: USize = 0
    while i < _size do
      _update(i, from(i))
      i = i + 1
    end

  new generate(g: {(): A^} val) =>
    var i: USize = 0
    while i < _size do
      _update(i, g())
      i = i + 1
    end

  new undefined[B: (A & Real[B] val & Number) = A]() =>
    """
    Create a vector of len elements, populating them with random memory. This
    is only allowed for a vector of numbers.
    """
    true

  fun size(): USize =>
    """
    Return the parameterised size, this lets us treat the Vector as a ReadSeq
    """
    _size

  fun apply(i: USize): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _apply(i)
    else
      error
    end

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    """
    if i < _size then
      _update(i, consume value)
    else
      error
    end

  fun add[_size': USize](other: Vector[this->A, _size']):
        Vector[this->A!, #(_size + _size')]^ =>
    """
    Create a new vector with references to the contents of this
    vector and the argument vector.
    """
    let result = Vector[this->A, #(_size + _size')]._create()
    var i: USize = 0
    while i < _size do
      result._update(i, _apply(i))
      i = i + 1
    end
    i = 0
    while i < _size' do
      result._update(i + _size, other._apply(i))
      i = i + 1
    end
    result

  fun keys(): VectorKeys[A, _size, this->Vector[A, _size]]^ =>
    """
    Return an iterator over the indices in the vector.
    """
    VectorKeys[A, _size, this->Vector[A, _size]]

  fun values(): VectorValues[A, _size, this->Vector[A, _size]]^ =>
    """
    Return an iterator over the values in the vector.
    """
    VectorValues[A, _size, this->Vector[A, _size]](this)

  fun pairs(): VectorPairs[A, _size, this->Vector[A, _size]]^ =>
    """
    Return an iterator over the pairs of indices and values in the vector.
    """
    VectorPairs[A, _size, this->Vector[A, _size]](this)

class VectorKeys[A, _size: USize, B: Vector[A, _size] #read] is Iterator[USize]
  var _i: USize

  new create() =>
    _i = 0

  fun has_next(): Bool =>
    _i < _size

  fun ref next(): USize =>
    if _i < _size then
      _i = _i + 1
    else
      _i
    end

class VectorValues[A, _size: USize, B: Vector[A, _size] #read] is Iterator[B->A]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < _size

  fun ref next(): B->A ? =>
    _vector(_i = _i + 1)

class VectorPairs[A, _size: USize, B: Vector[A, _size] #read] is Iterator[(USize, B->A)]
  let _vector: B
  var _i: USize

  new create(vector: B) =>
    _vector = vector
    _i = 0

  fun has_next(): Bool =>
    _i < _size

  fun ref next(): (USize, B->A) ? =>
    (_i, _vector(_i = _i + 1))
