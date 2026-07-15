import pytest

from oidscripts.debuggers.template_args import (
    TemplateTypeName,
    split_template_args,
)


def test_split_simple_eigen_matrix():
    name = 'Eigen::Matrix<float, -1, -1, 0, -1, -1>'
    assert split_template_args(name) == \
        ['float', '-1', '-1', '0', '-1', '-1']


def test_split_no_template_args():
    assert split_template_args('float') == []
    assert split_template_args('-1') == []


def test_split_nested_angle_brackets():
    name = ('Eigen::Map<Eigen::Matrix<float, -1, -1, 0, -1, -1>, 0, '
            'Eigen::Stride<0, 0> >')
    args = split_template_args(name)
    assert args == ['Eigen::Matrix<float, -1, -1, 0, -1, -1>', '0',
                    'Eigen::Stride<0, 0>']


def test_template_type_name_is_a_str():
    # Behaves as the plain type name everywhere it is used as a string.
    t = TemplateTypeName('Eigen::MatrixXf',
                         canonical='Eigen::Matrix<float, -1, -1, 0, -1, -1>')
    assert t == 'Eigen::MatrixXf'
    assert 'Map' not in t
    assert str(t) == 'Eigen::MatrixXf'


def test_template_argument_reads_from_canonical():
    t = TemplateTypeName('Eigen::MatrixXf',
                         canonical='Eigen::Matrix<float, -1, -1, 0, -1, -1>')
    assert str(t.template_argument(0)) == 'float'
    assert int(t.template_argument(1)) == -1
    assert int(t.template_argument(2)) == -1
    assert int(t.template_argument(3)) == 0


def test_template_argument_recurses_for_map():
    # Eigen::Map: arg 0 is the matrix type, whose own args are readable.
    canonical = ('Eigen::Map<Eigen::Matrix<float, 3, 4, 0, 3, 4>, 0, '
                 'Eigen::Stride<0, 0> >')
    t = TemplateTypeName('Eigen::Map<...>', canonical=canonical)
    matrix = t.template_argument(0)
    assert str(matrix.template_argument(0)) == 'float'
    assert int(matrix.template_argument(1)) == 3
    assert int(matrix.template_argument(2)) == 4
    # arg 2 is the stride type, whose inner stride is readable.
    stride = t.template_argument(2)
    assert int(stride.template_argument(1)) == 0


def test_template_argument_out_of_range_raises():
    t = TemplateTypeName('float')
    with pytest.raises(IndexError):
        t.template_argument(0)
