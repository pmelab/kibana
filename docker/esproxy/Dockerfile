FROM php:7-alpine
RUN curl -sS https://getcomposer.org/installer | php -- --install-dir=/usr/bin --filename=composer
COPY . /app
WORKDIR /app
RUN composer install
EXPOSE 9200
CMD ["php", "-S", "0.0.0.0:9200", "-t", "/app/web"]
