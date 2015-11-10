import logging

L = logging.getLogger('web')

import datetime as DT
import flask as F
import functools as FT
import hashlib
import hmac
import json
import os
import sqlalchemy as SA
import traceback

from sqlalchemy.orm.exc import NoResultFound
from sqlalchemy.orm.exc import MultipleResultsFound

from config import conf

import common
import db
import htmlify
import notify

app = F.Flask(__name__)
app.config['SERVER_NAME'] = str(conf.web.server.name)

def use_template(template_name):
    def decorator(f):
        @FT.wraps(f)
        def apply_template(*args, **kwargs):
            context = f(*args, **kwargs) or {}
            if not isinstance(context, dict):
                return context
            return F.render_template(template_name, **context)
        return apply_template
    return decorator

@app.after_request
def db_commit(request):
    db.session.commit()
    return request

@app.route('/-/<identifier>')
def p_shorturl(identifier):
    url = db.unshorten(identifier)
    if url is None:
        F.abort(404)
    return F.redirect(url)

@app.route('/')
@use_template('index.html')
def p_index():
    return { }

@app.route('/github', methods=['POST'])
def p_github():
    payload = F.request.data
    obj = json.loads(payload)
    event = F.request.headers.get('X-GitHub-Event', 'push')

    try:
        repository = db.session.query(db.Repository)\
            .filter(db.Repository.full_name == obj['repository']['full_name'])\
            .one()
    except NoResultFound:
        L.info('POST ignored (not a repo we care about)')
        return ('not a repo we care about', 404)

    if bool(int(conf.web.github.verify)):
        signature = F.request.headers['X-Hub-Signature']
        if not verify_github_signature(payload, repository.secret, signature):
            return ('unauthorized signature', 401)

    notify.notify_github(event, repository, obj)

    return ('got it!', 200)

def verify_github_signature(payload, secret, provided_signature):
    m = hmac.new(str(secret), str(payload), hashlib.sha1)
    computed_signature = 'sha1=' + m.hexdigest()
    print(computed_signature)
    print(provided_signature)
    return hmac.compare_digest(computed_signature, str(provided_signature))

common.initialize('web')
L.info('flask version: %s', F.__version__)

if __name__ == '__main__':
    app.run(
        debug=bool(int(conf.web.server.debug)),
        host=str(conf.web.server.host),
        )
