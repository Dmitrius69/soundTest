Тестовая программа для проверки работы со звуковыми библиотеками
libmpg123 и libao



Функция 
MPG123_EXPORT size_t mpg123_outblock(mpg123_handle*	mh) 	

Возвращает максимальный размер буффера для каждого фрейма, на которые разбит звуковой файл.
Используется для вычисления размера минимального буффера, который будет использован для считывания и декодировки 
звукового файла.

параметр
  mh  - handle (указатель) файла, который мы обрабатываем
 
Возвращаемое значение
  Возвращает максимальное значение размера одномоментно считываемых данный в байтах 



MPG123_EXPORT off_t mpg123_length(mpg123_handle* mh) 	

Return, if possible, the full (expected) length of current track in samples (PCM frames).

This relies either on an Info frame at the beginning or a previous call to mpg123_scan() to get the real number of 
MPEG frames in a file. It will guess based on file size if neither Info frame nor scan data are present. 
In any case, there is no guarantee that the decoder will not give you more data, for example in case the open file 
gets appended to during decoding.

Parameters
    mh	handle

Returns
    length >= 0 or MPG123_ERR if there is no length guess possibl

======================================================================================

QVector<QGraphicsPolygonItem *> polygons;
for(int i=0;i<100;i++) 
{
    QGraphicsPolygonItem * item = scene->addPolygon(poly, pen, brush);
    polygons.push_back(item);
}

А когда надо удалить элемент, то сначала вызывают для сцены метод removeItem, а потом освобождают память из под элемента с помощью delete

for(int i=0;i<100;i++) 
{
    scene->removeItem(polygons[i]);
    delete polygons[i];
}

Но если эти полигоны соответствуют уровням громкости звука с постоянными шагом по времени, то я бы лучше сразу создал нужное количество (например, 100 штук) и добавил их на сцену, также записав в вектор. А в процессе проигрывания звука просто изменял бы их размеры. Собственно зачем создавать новые, если можно просто старые поменять (они автоматически перерисуются при обновлении сцены).

// Создание сцены
QVector<QGraphicsRectItem *> rectangles;
for(int i=0;i<100;i++) 
{
    QGraphicsRectItem * item = scene->addRect(wBar*i, y, wBar, 0, pen, brush); // высота всех делений равна нулю
    polygons.push_back(item);
}

// Обновление сцены
QVector<QGraphicsRectItem *> rectangles;
for(int i=0;i<100;i++) 
{
    polygons[i]->setRect(wBar*i, y, wBar, VALUE[i], pen, brush); // высота i-го деления задается из массива VALUE
}

