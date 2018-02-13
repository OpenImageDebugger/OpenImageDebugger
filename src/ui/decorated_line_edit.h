#ifndef DECORATED_LINE_EDIT_H_
#define DECORATED_LINE_EDIT_H_

#include <QLabel>
#include <QLineEdit>

class DecoratedLineEdit : public QLineEdit
{
    Q_OBJECT

  public:
    DecoratedLineEdit(const char* icon_path,
                      const char* tooltip,
                      QWidget* parent = nullptr);

  private:
};

#endif // DECORATED_LINE_EDIT_H_
