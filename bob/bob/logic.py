import db

def create_repository(name, full_name, secret):
    return db.Repository(name=name, full_name=full_name, secret=secret)
