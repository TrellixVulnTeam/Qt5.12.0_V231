/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//! [0]
    class Y: public QEnableSharedFromThis<Y>
    {
    public:
        QSharedPointer<Y> f()
        {
            return sharedFromThis();
        }
    };

    int main()
    {
        QSharedPointer<Y> p(new Y());
        QSharedPointer<Y> y = p->f();
        Q_ASSERT(p == y); // p and q must share ownership
    }
//! [0]

//! [1]
    class ScriptInterface : public QObject
    {
        Q_OBJECT

        // ...

    public slots:
        void slotCalledByScript(Y *managedBySharedPointer)
        {
            QSharedPointer<Y> yPtr = managedBySharedPointer->sharedFromThis();
            // Some other code unrelated to scripts that expects a QSharedPointer<Y> ...
        }
    };
//! [1]

//! [2]
    static void doDeleteLater(MyObject *obj)
    {
        obj->deleteLater();
    }

    void otherFunction()
    {
        QSharedPointer<MyObject> obj =
            QSharedPointer<MyObject>(new MyObject, doDeleteLater);

        // continue using obj
        obj.clear();    // calls obj->deleteLater();
    }
//! [2]

//! [3]
    QSharedPointer<MyObject> obj =
        QSharedPointer<MyObject>(new MyObject, &QObject::deleteLater);
//! [3]

//! [4]
    if (sharedptr) { ... }
//! [4]

//! [5]
    if (!sharedptr) { ... }
//! [5]

//! [6]
    QSharedPointer<T> other(t); this->swap(other);
//! [6]

//! [7]
    QSharedPointer<T> other(t, deleter); this->swap(other);
//! [7]

//! [8]
    if (weakref) { ... }
//! [8]

//! [9]
    if (!weakref) { ... }
//! [9]

//! [10]
    qDebug("Tracking %p", weakref.data());
//! [10]

//! [11]
    // this pointer cannot be used in another thread
    // so other threads cannot delete it
    QWeakPointer<int> weakref = obtainReference();

    Object *obj = weakref.data();
    if (obj) {
        // if the pointer wasn't deleted yet, we know it can't get
        // deleted by our own code here nor the functions we call
        otherFunction(obj);
    }
//! [11]

//! [12]
    QWeakPointer<int> weakref;

    // ...

    QSharedPointer<int> strong = weakref.toStrongRef();
    if (strong)
        qDebug() << "The value is:" << *strong;
    else
        qDebug() << "The value has already been deleted";
//! [12]
